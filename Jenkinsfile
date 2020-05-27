pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                cmakeBuild buildType: 'Release', generator: 'Ninja', steps: [[args: 'all']]
            }
        }
        stage('Test') {
            steps {
                echo 'Testing..'
            }
        }
        stage('Deploy') {
            steps {
                echo 'Deploying....'
            }
        }
    }
}