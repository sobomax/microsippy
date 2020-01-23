node('microsippy') {
  timestamps {
    def builderHome = env.WORKSPACE
    def IDF_PATH = "${builderHome}/ESP8266_RTOS_SDK"
    def IDF_TOOLCHAIN = env.IDF_TOOLCHAIN
    stage('Prepare/Checkout') { // for display purposes
      dir('microsippy') {
        git branch: 'master', url: 'https://github.com/sobomax/microsippy.git'
      }
      dir('ESP8266_RTOS_SDK') {
        git branch: 'master', url: 'https://github.com/espressif/ESP8266_RTOS_SDK.git'
      }
    }

    stage('Clear Workspace') {
      sh "rm -rf ${builderHome}/microsippy/src/build"
    }

    stage('Build') {
      withCredentials([
       [$class: 'StringBinding', credentialsId:'2e13b399-dc4e-4d14-ae6e-1383b6d3e0ad',
        variable: 'WIFI_SSID'],
       [$class: 'StringBinding', credentialsId:'37542a38-ed60-45ed-87a8-33fd512ff740',
        variable: 'WIFI_PASSWORD']
       ]) {
        // Run the  build
        sh "IDF_PATH=${IDF_PATH} IDF_TOOLCHAIN=${IDF_TOOLCHAIN} ${builderHome}/microsippy/scripts/do-build.sh"
      }
    }

    stage('Flash') {
      // Flash the board
      sh "IDF_PATH=${IDF_PATH} IDF_TOOLCHAIN=${IDF_TOOLCHAIN} ${builderHome}/microsippy/scripts/do-build.sh flash"
    }

    stage('Test') {
      ansiColor('xterm') {
        sh "IDF_PATH=${IDF_PATH} IDF_TOOLCHAIN=${IDF_TOOLCHAIN} ${builderHome}/microsippy/scripts/do-test.sh"
      }
    }
  }
}

