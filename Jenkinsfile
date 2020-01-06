node('microsippy') {
  timestamps {
    def builderHome = env.WORKSPACE
    def bootISOArgs = env.BOOTISO_ARGS
    def bootISOEnv = env.BOOTISO_ENV
    stage('Prepare/Checkout') { // for display purposes
      //dir('tester') {
      //  git branch: 'master', credentialsId: '9657a408-4ce0-4930-91cb-c354b5ab0fd4',
      //   url: 'git@bitbucket.org:sippysoft/tester.git'
      //}
    }

    stage('Clear Workspace') {
      sh "rm -f ${builderHome}/src/build"
    }

    stage('Build') {
      // Run the  build
      sh "${builderHome}/scripts/do-build.sh"
    }
  }
}

