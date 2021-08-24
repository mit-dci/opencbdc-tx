package main

import (
	"context"
	_ "embed"
	"encoding/binary"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/jackc/pgx/v4"
	"github.com/jackc/pgx/v4/pgxpool"
)

//go:embed schema.sql
var schemaSql string

type key_value struct {
	key   uint16
	value interface{}
}

var pool *pgxpool.Pool
var read int64
var inserted int64

func main() {
	var err error
	pool, err = pgxpool.Connect(context.Background(), os.Getenv("DATABASE_URL"))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Unable to connect to database: %v\n", err)
		os.Exit(1)
	}
	defer pool.Close()

	if err := ensureTableStructure(); err != nil {
		panic(err)
	}

	currentDir, err := filepath.Abs(".")
	if err != nil {
		panic(err)
	}
	err = filepath.Walk(
		currentDir,
		func(path string, info os.FileInfo, err error) error {
			if !info.IsDir() {
				if strings.HasSuffix(path, ".bin") &&
					strings.Contains(info.Name(), "telemetry") {
					f, err := os.Open(path)
					if err != nil {
						panic(err)
					}
					err = readTelemetryFile(f)
					if err != nil {
						panic(err)
					}
				}
			}
			return nil
		},
	)
	if err != nil {
		panic(err)
	}
}

func ensureTableStructure() error {
	_, err := pool.Exec(context.Background(), schemaSql)
	return err
}

type telemetryPoint struct {
	tags        map[uint16]interface{}
	measurement uint16
	ts          int64
	dbmid       int64
}

var BatchSize = 500000

func readTelemetryFile(f *os.File) error {
	keys, revKeys, endPos, err := readKeys(f)
	if err != nil {
		return err
	}

	_, err = f.Seek(0, 0)
	if err != nil {
		return err
	}

	fileTags, err := readMap(f)
	if err != nil {
		return err
	}

	trid, trrid, err := insertTestRunAndRole(fileTags, revKeys)
	if err != nil {
		return err
	}

	pi := newPointInserter(trid, trrid, keys, revKeys)
	go pi.Run()
	pts := make([]telemetryPoint, 0)
	for {
		if read%10000 == 0 {
			fmt.Printf(
				"\rRead %d rows - inserted %d - now reading %s",
				read,
				inserted,
				f.Name(),
			)
		}
		pos, err := f.Seek(0, 1)
		if err != nil {
			return err
		}
		if pos == endPos {
			break
		}
		tp := uint16(0)
		if err := binary.Read(f, binary.LittleEndian, &tp); err != nil {
			return err
		}

		pointTags, err := readMap(f)
		if err != nil {
			return err
		}

		ts := int64(0)
		if err := binary.Read(f, binary.LittleEndian, &ts); err != nil {
			return err
		}

		pts = append(pts, telemetryPoint{
			ts:          ts,
			measurement: tp,
			tags:        pointTags,
		})

		if len(pts) >= BatchSize {
			pi.C <- pts
			inserted += int64(len(pts))
			pts = make([]telemetryPoint, 0)
		}
		read++
	}

	if len(pts) > 0 {
		pi.C <- pts
		inserted += int64(len(pts))
	}
	fmt.Printf("\rRead %d rows - inserted %d", read, inserted)
	pi.Close()
	return nil
}

type pointInserter struct {
	trid     int64
	trrid    int64
	fileKeys map[uint16]string
	revKeys  map[string]uint16
	C        chan []telemetryPoint
	wg       sync.WaitGroup
}

func newPointInserter(trid int64,
	trrid int64,
	fileKeys map[uint16]string,
	revKeys map[string]uint16) *pointInserter {
	return &pointInserter{
		trid:     trid,
		trrid:    trrid,
		fileKeys: fileKeys,
		revKeys:  revKeys,
		C:        make(chan []telemetryPoint),
		wg:       sync.WaitGroup{},
	}
}

func (pi *pointInserter) Close() {
	pi.wg.Add(1)
	close(pi.C)
	// Ensure goroutine finished before returning, otherwise the
	// main() will close the connection pool from under us.
	pi.wg.Wait()
}

func (pi *pointInserter) Run() {
	for points := range pi.C {
		var err error
		fields := []string{
			"trid",
			"trrid",
			"mid",
			"ts",
			"tn",
			"txid",
			"latency",
			"outcome",
			"txaddress",
			"txaddress2",
			"stokey",
			"stovalue",
			"codeoffset",
			"locktype",
			"tn2",
			"stokey2",
		}
		rows := make([][]interface{}, len(points))

		for i := range points {
			points[i].dbmid, err = insertMeasurement(
				pi.fileKeys[points[i].measurement],
			)
			if err != nil {
				panic(err)
			}
			rows[i] = make([]interface{}, len(fields))
			rows[i][0] = pi.trid
			rows[i][1] = pi.trrid
			rows[i][2] = points[i].dbmid
			rows[i][3] = points[i].ts
			for k, v := range points[i].tags {
				key, ok := pi.fileKeys[k]
				if !ok {
					fmt.Printf("Unspecified key used: %d", k)
					os.Exit(3)
				}
				switch key {
				case "ticket_number":
					rows[i][4] = v.(uint64)
				case "txid":
					rows[i][5] = v.([]byte)
				case "latency":
					rows[i][6] = v.(int64)
				case "outcome":
					rows[i][7] = v.(uint8)
				case "address":
					rows[i][8] = v.([]byte)
				case "address2":
					rows[i][9] = v.([]byte)
				case "storagekey":
					rows[i][10] = v.([]byte)
				case "storagevalue":
					rows[i][11] = v.([]byte)
				case "codeoffset":
					rows[i][12] = v.(uint64)
				case "locktype":
					rows[i][13] = v.(uint8)
				case "ticket_number2":
					rows[i][14] = v.(uint64)
				case "storagekey2":
					rows[i][15] = v.([]byte)
				}
			}
		}

		count, err := pool.CopyFrom(
			context.Background(),
			pgx.Identifier{"telemetry_point"},
			fields,
			pgx.CopyFromRows(rows),
		)
		if err != nil {
			panic(err)
		}
		if count != int64(len(points)) {
			panic(fmt.Sprintf("Only copied %d of %d rows", count, len(points)))
		}
	}
	pi.wg.Done()
}

func insertTestRunAndRole(
	fileTags map[uint16]interface{},
	revKeys map[string]uint16,
) (int64, int64, error) {
	testRunID := fileTags[revKeys["testrun_id"]].(string)
	testRunRole := fileTags[revKeys["testrun_role"]].(string)
	awsInstance := fileTags[revKeys["aws_instance"]].(string)
	awsRegion := fileTags[revKeys["aws_region"]].(string)

	trID, err := insertTestRun(testRunID)
	if err != nil {
		return -1, -1, err
	}

	regionID, err := insertRegion(awsRegion)
	if err != nil {
		return -1, -1, err
	}

	trrID, err := insertTestRunRole(trID, regionID, testRunRole, awsInstance)
	if err != nil {
		return -1, -1, err
	}

	return trID, trrID, nil
}

func insertTestRun(id string) (int64, error) {
	var dbid int64
	err := pool.QueryRow(context.Background(), "SELECT id FROM testrun WHERE testcontroller_id=$1", id).
		Scan(&dbid)
	if err == nil {
		return dbid, nil
	}
	err = pool.QueryRow(context.Background(), "INSERT INTO testrun(testcontroller_id) VALUES ($1) RETURNING id", id).
		Scan(&dbid)
	if err != nil {
		return -1, err
	}
	return dbid, nil
}

var midCache = map[string]int64{}

func insertMeasurement(name string) (int64, error) {
	dbid, ok := midCache[name]
	if ok {
		return dbid, nil
	}
	err := pool.QueryRow(context.Background(), "SELECT id FROM measurement WHERE name=$1", name).
		Scan(&dbid)
	if err == nil {
		return dbid, nil
	}
	err = pool.QueryRow(context.Background(), "INSERT INTO measurement(name) VALUES ($1) RETURNING id", name).
		Scan(&dbid)
	if err != nil {
		return -1, err
	}
	midCache[name] = dbid
	return dbid, nil
}

func insertRegion(name string) (int64, error) {
	var dbid int64
	err := pool.QueryRow(context.Background(), "SELECT id FROM region WHERE name=$1", name).
		Scan(&dbid)
	if err == nil {
		return dbid, nil
	}
	err = pool.QueryRow(context.Background(), "INSERT INTO region(name) VALUES ($1) RETURNING id", name).
		Scan(&dbid)
	if err != nil {
		return -1, err
	}
	return dbid, nil
}

func insertTestRunRole(
	trid int64,
	regionid int64,
	role, awsInstance string,
) (int64, error) {
	var dbid int64
	err := pool.QueryRow(context.Background(),
		`SELECT id FROM testrunrole
	 	 WHERE testrun_id=$1 and role_name=$2`,
		trid,
		role,
	).Scan(&dbid)
	if err == nil {
		return -1, errors.New(
			"test run role already exists - duplicate import?",
		)
	}
	err = pool.QueryRow(context.Background(),
		`INSERT INTO testrunrole(testrun_id, role_name, instance_id, rid)
		 VALUES ($1,$2,$3,$4)
		 RETURNING id`,
		trid,
		role,
		awsInstance,
		regionid,
	).Scan(&dbid)
	if err != nil {
		return -1, err
	}
	return dbid, nil
}

func readKeys(f *os.File) (map[uint16]string, map[string]uint16, int64, error) {
	keys := map[uint16]string{}
	revKeys := map[string]uint16{}
	// Get the last 8 bytes
	f.Seek(-8, 2)
	keysStartPos := int64(0)
	binary.Read(f, binary.LittleEndian, &keysStartPos)

	f.Seek(keysStartPos, 0)
	// Read key count
	keyCount := int64(0)
	binary.Read(f, binary.LittleEndian, &keyCount)

	for i := int64(0); i < keyCount; i++ {
		keyLen := int64(0)
		binary.Read(f, binary.LittleEndian, &keyLen)
		b := make([]byte, keyLen)
		n, err := f.Read(b)
		if err != nil {
			return nil, nil, 0, err
		}
		if n != len(b) {
			return nil, nil, 0, fmt.Errorf(
				"failed to read %d bytes - got %d",
				len(b),
				n,
			)
		}
		numKey := uint16(0)
		binary.Read(f, binary.LittleEndian, &numKey)
		keys[numKey] = string(b)
		revKeys[string(b)] = numKey
	}
	return keys, revKeys, keysStartPos, nil
}

func readMap(f *os.File) (map[uint16]interface{}, error) {
	res := map[uint16]interface{}{}
	keyCount := int64(0)
	binary.Read(f, binary.LittleEndian, &keyCount)
	if keyCount == 0 {
		return nil, fmt.Errorf("entry has no keys")
	}
	for i := int64(0); i < keyCount; i++ {
		numKey := uint16(0)
		binary.Read(f, binary.LittleEndian, &numKey)

		valueType := int8(0)
		binary.Read(f, binary.LittleEndian, &valueType)

		// FROM telemetry.hpp:
		// using telemetry_value
		// = std::variant<int64_t, std::string, cbdc::hash_t, uint8_t,
		// uint64_t>;

		var value interface{}

		switch valueType {
		case 0: //int64_t
			var val int64
			binary.Read(f, binary.LittleEndian, &val)
			value = val
		case 1: //std::string
			valLen := int64(0)
			binary.Read(f, binary.LittleEndian, &valLen)
			b := make([]byte, valLen)
			n, err := f.Read(b)
			if err != nil {
				return nil, err
			}
			if n != len(b) {
				return nil, fmt.Errorf(
					"failed to read %d bytes - got %d",
					len(b),
					n,
				)
			}
			value = string(b)
		case 2: //cbdc::hash_t
			val := make([]byte, 32)
			n, err := f.Read(val)
			if err != nil {
				return nil, err
			}
			if n != len(val) {
				return nil, fmt.Errorf(
					"failed to read %d bytes - got %d",
					len(val),
					n,
				)
			}
			value = val
		case 3: // uint8_t
			var val uint8
			binary.Read(f, binary.LittleEndian, &val)
			value = val
		case 4: //uint64_t
			var val uint64
			binary.Read(f, binary.LittleEndian, &val)
			value = val
		case 5: //cbdc::buffer
			valLen := int64(0)
			binary.Read(f, binary.LittleEndian, &valLen)
			b := make([]byte, valLen)
			n, err := f.Read(b)
			if err != nil {
				return nil, err
			}
			if n != len(b) {
				return nil, fmt.Errorf(
					"failed to read %d bytes - got %d",
					len(b),
					n,
				)
			}
			value = b
		}

		if value == nil {
			return nil, fmt.Errorf("unknown value type %d", valueType)
		}

		res[numKey] = value
	}
	return res, nil
}
