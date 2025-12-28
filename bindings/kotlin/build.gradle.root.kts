// Root build file for standalone builds of kryon bindings
// When building bindings/kotlin as part of a multi-module project,
// the parent project should declare these plugin versions with 'apply false'

buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath("com.android.tools.build:gradle:8.2.0")
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:1.9.20")
    }
}

allprojects {
    repositories {
        google()
        mavenCentral()
    }
}
