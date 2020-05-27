pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                cmakeBuild cleanBuild: true, installation: 'InSearchPath', buildType: 'Release', generator: 'Ninja', steps: [[args: 'all']]
            }
        }
        stage('Test') {
            steps {
                sh 'test/odr_test'
            }
        }
    }
}
