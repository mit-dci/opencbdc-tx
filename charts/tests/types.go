package test

import (
	"fmt"

	"github.com/gruntwork-io/terratest/modules/k8s"
)

type ImageConfig struct {
	Repository string `default:"opencbdc-tx"`
	Tag        string `default:"latest"`
	PullPolicy string `default:"Never" split_words:"true"`
}

type TestRunConfig struct {
	Id                       string `default:"testrun_id"`
	DelaySecondsAfterInstall int    `default:"60" split_words:"true"`
}

type GlobalConfig struct {
	RunSuitesInParallel bool `default:"false"`
}

type Container struct {
	name        string
	podName     string
	namespace   string
	testrunName string
}

func (c *Container) LogFileName() string {
	return fmt.Sprintf("%s_%s_%s_%s.log", c.testrunName, c.namespace, c.podName, c.name)
}

func (c *Container) GetAndSaveContainerLogs(s *E2ETestSuite) error {
	// Get pod logs to store for later
	out, err := k8s.RunKubectlAndGetOutputE(s.T(), s.kubectlOptions, "logs", fmt.Sprintf("pod/%s", c.podName), "-c", c.name)
	if err != nil {
		return fmt.Errorf("Error collecting logs from container '%s' in pod '%s': %v", c.name, c.podName, err)
	}
	err = s.writeContainerLogFile(c.LogFileName(), out, fmt.Sprintf("../../testruns/%s/logs", s.config.Id))
	if err != nil {
		return fmt.Errorf("Error writing logfile '%s': %v", c.LogFileName(), err)
	}
	return nil
}
