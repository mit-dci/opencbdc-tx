package test

import (
	"errors"
	"fmt"
	"os"

	"github.com/gruntwork-io/terratest/modules/logger"
)

func (s *E2ETestSuite) writeContainerLogFile(filename string, content string, logdir string) error {
	if _, err := os.Stat(logdir); errors.Is(err, os.ErrNotExist) {
		err := os.MkdirAll(logdir, os.ModePerm)
		if err != nil {
			return fmt.Errorf("Error while creating directory '%s': %v", logdir, err)
		}
	}
	logfile := fmt.Sprintf("%s/%s", logdir, filename)

	f, err := os.Create(logfile)
	if err != nil {
		return fmt.Errorf("Error while creating logfile '%s': %v", logfile, err)
	}

	l, err := f.WriteString(content)
	if err != nil {
		f.Close()
		return fmt.Errorf("Error while writing content to logfile '%s': %v", logfile, err)
	}

	logger.Log(s.T(), fmt.Sprintf("%d bytes written to %s", l, logfile))

	err = f.Close()
	if err != nil {
		return fmt.Errorf("Error closing logfile '%s': %v", logfile, err)
	}

	return nil
}
