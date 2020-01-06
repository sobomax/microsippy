node('microsippy') {
  timestamps {
    def builderHome = env.WORKSPACE
    def bootISOArgs = env.BOOTISO_ARGS
    def bootISOEnv = env.BOOTISO_ENV
    stage('Prepare/Checkout') { // for display purposes
      dir('microsippy') {
        git branch: 'master', url: 'https://github.com/sobomax/microsippy.git'
      }
    }

    stage('Clear Workspace') {
      sh "rm -f ${builderHome}/src/build"
    }

    stage('Build') {
      // Run the  build
      sh "${builderHome}/microsippy/scripts/do-build.sh"
    }
  }
}

