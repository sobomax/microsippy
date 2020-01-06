node('microsippy') {
  timestamps {
    def builderHome = env.WORKSPACE
    def IDF_PATH = env.IDF_PATH
    def IDF_TOOLCHAIN = env.IDF_TOOLCHAIN
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
      sh "IDF_PATH=${IDF_PATH} IDF_TOOLCHAIN=${IDF_TOOLCHAIN} ${builderHome}/microsippy/scripts/do-build.sh"
    }

    stage('Flash') {
      // Flash the board
      sh "IDF_PATH=${IDF_PATH} IDF_TOOLCHAIN=${IDF_TOOLCHAIN} ${builderHome}/microsippy/scripts/do-build.sh flash"
    }
  }
}

