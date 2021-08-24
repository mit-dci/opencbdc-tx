DROP TABLE IF EXISTS measurement;
DROP TABLE IF EXISTS region;
DROP TABLE IF EXISTS telemetry_point;
DROP TABLE IF EXISTS testrun;
DROP TABLE IF EXISTS testrunrole;

CREATE TABLE IF NOT EXISTS testrun(
    id serial primary key,
    testcontroller_id varchar(40)
);

CREATE TABLE IF NOT EXISTS region(
    id serial primary key,
    name varchar(20)
);

CREATE TABLE IF NOT EXISTS testrunrole(
    id bigserial primary key,
    testrun_id int,
    rid int,
    role_name varchar(25),
    instance_id varchar(25)
);

CREATE TABLE IF NOT EXISTS measurement(
    id serial primary key,
    name varchar(40)
);

CREATE TABLE IF NOT EXISTS telemetry_point(
    trid bigint,
    trrid bigint,
    ts bigint,
    mid int,
    tn bigint,
    txid bytea,
    latency bigint,
    outcome smallint,
    txaddress bytea,
    txaddress2 bytea,
    stokey bytea,
    stovalue bytea,
    codeoffset int,
    locktype smallint,
    tn2 bigint,
    stokey2 bytea
);-- PARTITION BY LIST(trid); -- TODO for large datasets we might consider partitioning

CREATE INDEX testrun_id on testrun(testcontroller_id);
CREATE INDEX testrun_role on testrunrole(testrun_id, role_name);
CREATE INDEX region_name on region(name);
CREATE INDEX measurement_name on measurement(name);
CREATE INDEX telemetry_point_mid ON telemetry_point(trid, mid);
CREATE INDEX telemetry_point_txid ON telemetry_point(trid, txid);
CREATE INDEX telemetry_point_tn ON telemetry_point(trid, tn);
CREATE INDEX telemetry_point_txaddr ON telemetry_point(trid, txaddress);
CREATE INDEX telemetry_point_stokey ON telemetry_point(trid, stokey);

