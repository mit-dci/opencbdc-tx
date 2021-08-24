# Telemetry

The telemetry system offers a way to track transactions across all components
in the system. It works similar to our logging system: in the main process a
telemetry logger is created as a shared_ptr and passed around to the components
that need to log telemetry points.

Recipients of the shared_ptr should cope with it being a nullptr when telemetry
is not used.

In those components, a telemetry point can be logged containing:

- The event that happened (measurement)
- The involved ticket number and/or transaction id (note: at least one event needs to define both such that they can be mapped)
- The latency of the event / measurement
- (optional) an outcome. This is sensible for measurements of a function that can have different outcomes, which are expected to have different latencies

The file is written in a binary format that is sequenced like this:

- map<uint16_t, telemetry_value> metadata
- array of uint16_t - vector<pair<uint16_t, telemetry_value>> (telemetry details) - int64_t (timestamp)
- map<uint16_t, std::string> key_dictionary
- size_t start of key_dictionary

metadata contains the information about the telemetry file: testrun id, role, aws instance and region.

Everywhere in the file where a uint16_t is used, this is expected to be an entry
in the key dictionary. More about this is explained in telemetry.hpp

telemetry_value can be a string, uint8, uint64, int64 or cbdc::hash. This is also
further detailed in telemetry.hpp

# Using telemetry

In the test controller, there's an added checkbox "Enable telemetry" which will
switch on telemetry when a branch of the code is used in which this is included.

It will pass --telemetry=1 to all components, which will trigger the phase two
components to construct a telemetry logger and start adding events to it.

The agent-outputs S3 bucket will get all data from the test run roles after a
test run has completed, and it will contain telemetry files for each role.

Download these files into a folder. For instance for test run 1b3164cc0531 from the test environment this
looks like this:

```
aws s3 cp s3://705536427507-us-east-1-agent-outputs/testruns/1b3164cc0531/outputs/ . --recursive --exclude "*" --include "*.bin"
```

While in that folder, run the `scripts/telemetry-processor/process-telemetry.sh` script.

This will stand up a (temporary) Postgres database server in Docker, together with a pgadmin instance.

It will then proceed to load all telemetry files into the database. The database schema is in `scripts/telemetry-processor/schema.sql`

After loading all telemetry files, the script will launch a psql prompt from the docker container, enabling you to query the telemetry data

TODO: Given the volume of data, it is interesting to investigate how to best run this inside AWS through an ephemeral EC2 instance, since the transfer of telemetry data to a local development machine is then no longer needed.

**NOTE**: You cannot run two sets of telemetry data side-by-side as the telemetry script will teardown the database before loading new telemetry data. So be aware that running another copy of the telemetry script while you're in the psql console for the previous one will terminate that.

## Queries:

### Get 99.9% latency

```
SELECT
    percentile_disc(0.999)
    WITHIN GROUP (ORDER BY telemetry_point.latency)
FROM
    telemetry_point
WHERE
    mid=(SELECT id FROM measurement WHERE name='confirm_transaction');
```

### Query TXIDs that are over 99.9% latency

```
SELECT
    txid
FROM
    telemetry_point
WHERE
    mid=(SELECT id FROM measurement WHERE name='confirm_transaction')
    AND latency > (
        SELECT
            percentile_disc(0.999)
            WITHIN GROUP (ORDER BY telemetry_point.latency)
        FROM
            telemetry_point
        WHERE
            mid=(SELECT id FROM measurement WHERE name='confirm_transaction')
    );
```

### Query trace of a single TXID

( replace `\xfdefaf474c037f757b55a4d3a3116b2ea8db6ed889a6593b31748d89351918ff` with one of the TXIDs from the query above )

```
SELECT tp.tn, tp.txid, trr.role_name as role, r.name as region, m.name as measurement, tp.ts, tp.outcome, tp.latency, tp.txaddress, tp.txaddress2, tp.stokey, tp.stovalue, tp.codeoffset, tp.locktype
FROM
    (SELECT
        DISTINCT txid, tn
    FROM
        telemetry_point
    WHERE
        mid=(
            SELECT
                mid
            FROM
                telemetry_point
            WHERE
                txid IS NOT NULL
                AND tn IS NOT NULL
            LIMIT 1
        )
        AND txid='\xfdefaf474c037f757b55a4d3a3116b2ea8db6ed889a6593b31748d89351918ff'
    ) AS txtn
    LEFT JOIN telemetry_point tp ON (tp.txid = txtn.txid OR tp.tn=txtn.tn)
    LEFT JOIN testrunrole trr on trr.id=tp.trrid
    LEFT JOIN measurement m on m.id=tp.mid
    LEFT JOIN region r on r.id=trr.rid
ORDER BY tp.ts;
```

### Query all ticket traces that touch a particular storage key

( replace `\x6e3eed104391f66914c0c9bc8faaf674a8b79b5446716ac75f17e28a8c9b22a4cab6947b97a35932ebff214ac493384ece09691e` with a storage key that has a lock contention distilled from the TX trace from the single TX )

( enable the `WHERE` clause to only see locking operation on the storage key without all the other traces )

```
SELECT
    txtn.tn,
    encode(txtn.txid, 'hex') as txid,
    trr.role_name as role,
    r.name as region,
    m.name as measurement,
    tp.ts,
    tp.outcome,
    tp.latency,
    encode(tp.txaddress, 'hex') as txaddress,
    encode(tp.txaddress2, 'hex') as txaddress2,
    encode(tp.stokey, 'hex') as storagekey,
    encode(tp.stovalue, 'hex') as storagevalue,
    tp.codeoffset,
    tp.locktype
FROM
    (
        SELECT
            DISTINCT txid,
            tn
        FROM
            telemetry_point
        WHERE
            mid =(
                SELECT
                    mid
                FROM
                    telemetry_point
                WHERE
                    txid IS NOT NULL
                    AND tn IS NOT NULL
                LIMIT
                    1
            )
            AND txid IN(SELECT distinct txid FROM telemetry_point WHERE stokey='\x6e3eed104391f66914c0c9bc8faaf674a8b79b5446716ac75f17e28a8c9b22a4cab6947b97a35932ebff214ac493384ece09691e')
    ) AS txtn
    LEFT JOIN telemetry_point tp ON (
        tp.txid = txtn.txid
        OR tp.tn = txtn.tn
    )
    LEFT JOIN testrunrole trr on trr.id = tp.trrid
    LEFT JOIN measurement m on m.id = tp.mid
    LEFT JOIN region r on r.id = trr.rid
-- WHERE m.name='broker_try_lock' and stokey='\x6e3eed104391f66914c0c9bc8faaf674a8b79b5446716ac75f17e28a8c9b22a4cab6947b97a35932ebff214ac493384ece09691e'
ORDER BY
    tp.ts;
```
