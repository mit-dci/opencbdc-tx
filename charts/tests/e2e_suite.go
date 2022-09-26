package test

import (
	"fmt"
	"path/filepath"
	"strings"
	"time"

	"github.com/gruntwork-io/terratest/modules/helm"
	"github.com/gruntwork-io/terratest/modules/k8s"
	"github.com/gruntwork-io/terratest/modules/logger"
	"github.com/gruntwork-io/terratest/modules/random"
	"github.com/kelseyhightower/envconfig"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"
	corev1 "k8s.io/api/core/v1"
	v1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

type E2ETestSuite struct {
	suite.Suite
	name            string
	image           ImageConfig
	kubectlOptions  *k8s.KubectlOptions
	helmOptions     *helm.Options
	namespace       string
	helmChartPath   string
	helmValuesFiles []string
	helmReleaseName string
	containers      []Container
	config          TestRunConfig
}

func NewE2ETestSuite(name string, helmChartPath string, helmValuesFiles []string) *E2ETestSuite {
	return &E2ETestSuite{name: name, helmChartPath: helmChartPath, helmValuesFiles: helmValuesFiles}
}

func (s *E2ETestSuite) LoadImgFromEnv() error {
	err := envconfig.Process("opencbdc_image", &s.image)
	if err != nil {
		return fmt.Errorf("Error occurred loading image config from environment: %v", err)
	}
	return nil
}

func (s *E2ETestSuite) LoadTestRunConfigFromEnv() error {
	err := envconfig.Process("testrun", &s.config)
	if err != nil {
		return fmt.Errorf("Error occurred loading testrun config from environment: %v", err)
	}
	return nil
}

func (s *E2ETestSuite) SetupSuite() {
	// Load default overrides from environment
	err := s.LoadImgFromEnv()
	s.Require().NoError(err)

	err = s.LoadTestRunConfigFromEnv()
	s.Require().NoError(err)

	// Define a unique namespace where Kubernetes/Helm resources will be installed
	s.namespace = fmt.Sprintf("opencbdc-tx-%s", strings.ToLower(random.UniqueId()))

	// Configure kubectl client to use unique namespace
	s.kubectlOptions = k8s.NewKubectlOptions("", "", s.namespace)

	// Resolve path to Helm chart and make sure it exists
	helmChartAbsolutePath, err := filepath.Abs(s.helmChartPath)
	s.Require().NoError(err)

	// Define Helm configuration and set a few values necessary for testing
	s.helmOptions = &helm.Options{
		KubectlOptions: s.kubectlOptions,
		ValuesFiles:    s.helmValuesFiles,
		SetValues: map[string]string{
			"image.repository":       s.image.Repository,
			"image.tag":              s.image.Tag,
			"image.pullPolicy":       s.image.PullPolicy,
			"enableClient":           "true",
			"config.defaultLogLevel": "DEBUG",
		},
	}

	// Create new unique Kubernetes namespace
	k8s.CreateNamespace(s.T(), s.kubectlOptions, s.namespace)

	// Define unique Helm release name
	s.helmReleaseName = fmt.Sprintf(
		"opencbdc-tx-%s",
		strings.ToLower(random.UniqueId()),
	)

	// Install Helm release
	helm.Install(s.T(), s.helmOptions, helmChartAbsolutePath, s.helmReleaseName)

	// Sleep for a duration after installing Helm release to give components time to start or fail to start
	time.Sleep(time.Duration(s.config.DelaySecondsAfterInstall) * time.Second)
}

func (s *E2ETestSuite) TearDownSuite() {
	// Iterate through all previously observed containers and collect logs for reference after tests complete
	for _, c := range s.containers {
		err := c.GetAndSaveContainerLogs(s)
		s.Require().NoError(err)
	}

	// Clean up Helm release by uninstalling from Kubernetes
	helm.Delete(s.T(), s.helmOptions, s.helmReleaseName, true)

	// Clean up and remove Kubernetes namespace
	k8s.DeleteNamespace(s.T(), s.kubectlOptions, s.namespace)
}

func (s *E2ETestSuite) TestPodsReady() {
	// Get all pods and iterate through to determine whether or not they're running correctly.
	for _, pod := range k8s.ListPods(s.T(), s.kubectlOptions, v1.ListOptions{}) {
		// Get all containers for each pod and determine whether or not they're running correctly
		for _, containerStatus := range pod.Status.ContainerStatuses {

			// Store container info for use in TearDownSuite step.
			container := Container{
				name:        containerStatus.Name,
				podName:     pod.ObjectMeta.Name,
				namespace:   s.namespace,
				testrunName: s.name,
			}
			s.containers = append(s.containers, container)

			// Make sure Pod is in "Running" phase
			assert.Equal(
				s.T(), corev1.PodPhase("Running"), pod.Status.Phase,
				fmt.Sprintf("%s is not ready...", pod.ObjectMeta.Name),
			)

			// Make sure container has not restarted and is ready
			assert.Equal(
				s.T(), int32(0), containerStatus.RestartCount,
				fmt.Sprintf(
					"container '%s' in pod '%s' has restarted %d time(s)! Check log file: %s",
					containerStatus.Name, pod.ObjectMeta.Name, containerStatus.RestartCount, container.LogFileName(),
				),
			)

			// Make sure container status is "Ready"
			assert.Equal(
				s.T(), true, containerStatus.Ready,
				fmt.Sprintf(
					"container '%s' in pod '%s' is not ready... Check log file: %s",
					containerStatus.Name, pod.ObjectMeta.Name, container.LogFileName(),
				),
			)
		}
	}
}

func (s *E2ETestSuite) TestMintAndSend() {
	// Mint outputs for wallet0
	logger.Log(s.T(), "Minting outputs for wallet 0")
	mintOut, _ := k8s.RunKubectlAndGetOutputE(
		s.T(), s.kubectlOptions,
		"exec", "-i", fmt.Sprintf("%s-client", s.helmReleaseName), "--",
		"build/src/uhs/client/client-cli",
		"/usr/share/config/2pc.cfg",
		"mempool0.dat",
		"wallet0.dat",
		"mint",
		"10",
		"5",
	)

	// Get wallet0 address from mintOut and store value
	mintOutSplit := strings.Split(mintOut, "\n")
	wallet0Address := mintOutSplit[len(mintOutSplit)-1]
	logger.Log(s.T(), fmt.Sprintf("Wallet0 Address: %s", wallet0Address))

	// Check wallet0 balance and make sure balance is correct
	logger.Log(s.T(), "Checking wallet0 balance...")
	infoOut, _ := k8s.RunKubectlAndGetOutputE(
		s.T(), s.kubectlOptions,
		"exec", "-i", fmt.Sprintf("%s-client", s.helmReleaseName), "--",
		"build/src/uhs/client/client-cli",
		"/usr/share/config/2pc.cfg",
		"mempool0.dat",
		"wallet0.dat",
		"info",
	)
	assert.Contains(s.T(), "Balance: $0.50, UTXOs: 10, pending TXs: 0", infoOut, "Minting failed!")

	// Create wallet1
	logger.Log(s.T(), "Creating wallet 1")
	createWallet1Out, _ := k8s.RunKubectlAndGetOutputE(
		s.T(), s.kubectlOptions,
		"exec", "-i", fmt.Sprintf("%s-client", s.helmReleaseName), "--",
		"build/src/uhs/client/client-cli",
		"/usr/share/config/2pc.cfg",
		"mempool1.dat",
		"wallet1.dat",
		"newaddress",
	)

	// Get wallet address from createWallet1Out and store value
	createWallet1OutSplit := strings.Split(createWallet1Out, "\n")
	wallet1Address := createWallet1OutSplit[len(createWallet1OutSplit)-1]
	logger.Log(s.T(), fmt.Sprintf("Wallet1 Address: %s", wallet1Address))

	// Send tx from wallet0 to wallet1
	logger.Log(s.T(), "Sending tx from wallet0 wallet1")
	sendTxOut, _ := k8s.RunKubectlAndGetOutputE(
		s.T(), s.kubectlOptions,
		"exec", "-i", fmt.Sprintf("%s-client", s.helmReleaseName), "--",
		"build/src/uhs/client/client-cli",
		"/usr/share/config/2pc.cfg",
		"mempool0.dat",
		"wallet0.dat",
		"send",
		"30",
		wallet1Address,
	)
	assert.Contains(s.T(), sendTxOut, "Sentinel responded: Confirmed")
}
