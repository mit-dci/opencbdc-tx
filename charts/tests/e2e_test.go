package test

import (
	"log"
	"testing"

	"github.com/kelseyhightower/envconfig"
	"github.com/stretchr/testify/suite"
)

func Test2PC(t *testing.T) {
	var c GlobalConfig
	err := envconfig.Process("e2e_test", &c)
	if err != nil {
		log.Fatalf("Error occurred loading global config from environment: %v", err)
	}

	cases := []struct {
		description     string
		helmValuesFiles []string
	}{
		// TODO: uncomment this later. Currently there is problem with starting coordinator clients in
		// sentinels with a non-replicated configuration
		// {
		// 	description:     "non-replicated",
		// 	helmValuesFiles: []string{},
		// },
		{
			description:     "replicated",
			helmValuesFiles: []string{"./test-values/2pc-replicated.yaml"},
		},
		{
			description:     "replicated-multi-cluster",
			helmValuesFiles: []string{"./test-values/2pc-replicated-multi-cluster.yaml"},
		},
	}

	for _, tt := range cases {
		tt := tt
		t.Run(tt.description, func(t *testing.T) {
			if c.RunSuitesInParallel {
				t.Parallel()
			}
			s := NewE2ETestSuite(tt.description, "../opencbdc-2pc", tt.helmValuesFiles)
			suite.Run(t, s)
		})
	}
}
