pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                cmakeBuild
                    installation: 'InSearchPath',
                    buildType: 'Release',
                    generator: 'Ninja',
                    buildDir: 'build',
                    cleanBuild: true,
                    steps: [[args: 'all']]
            }
        }
        stage('Test') {
            steps {
                sh 'build/test/odr_test'
            }
        }
    }
}
