// Evaluate the actual optional scripts with a small Gradle DSL fixture.
// Verify the selected sources, manifests and dependencies, not source spelling.
class GradleException extends RuntimeException { GradleException(String s) { super(s) } }
class Pick { def all() { this } }
File repo = new File(args[0])
for (edition in ['direct', 'fdroid', 'gplay']) {
    def dependencies = []
    def javaDirs = []
    def manifests = []
    def applied = []
    def props = ['kryon.dir': repo.path, 'app.edition': edition]
    def android = new Expando(sourceSets: new Expando(main: new Expando(java: new Expando(
        srcDir: { File path -> javaDirs << path }))),
        defaultConfig: new Expando(manifestPlaceholders: [:]))
    Binding binding = new Binding()
    binding.setVariable('project', new Expando(findProperty: { String key -> props[key] }))
    binding.setVariable('android', android)
    binding.setVariable('onVariants', { pick, Closure callback ->
        callback(new Expando(sources: new Expando(manifests: new Expando(addStaticManifestFile: { String path -> manifests << new File(path) }))))
    })
    binding.setVariable('androidComponents', { Closure callback -> callback() })
    binding.setVariable('selector', { -> new Pick() })
    binding.setVariable('implementation', { String dep -> dependencies << dep })
    binding.setVariable('dependencies', { Closure closure -> closure() })
    GroovyShell shell = new GroovyShell(this.class.classLoader, binding)
    binding.setVariable('apply', { Map opts ->
        File script = opts.from
        applied << script
        binding.setVariable('buildscript', new Expando(sourceFile: script))
        shell.evaluate(script)
    })
    File script = new File(repo, 'templates/android-notifications-push.gradle')
    binding.setVariable('buildscript', new Expando(sourceFile: script))
    shell.evaluate(script)
    assert javaDirs.every { it.isDirectory() }
    assert manifests.size() == 1 && manifests[0].isFile()
    if (edition == 'gplay') {
        assert dependencies == ['com.google.firebase:firebase-messaging:25.1.2']
        assert javaDirs.any { it.path.contains('/gplay/java') }
        assert !javaDirs.any { it.path.contains('/unified/java') }
    } else {
        assert dependencies == ['org.unifiedpush.android:connector:3.3.3-rc1']
        assert applied.empty
        assert javaDirs.any { it.path.contains('/unified/java') }
        assert !javaDirs.any { it.path.contains('/gplay/java') }
        javaDirs.each { dir -> dir.eachFileRecurse { file ->
            if (file.isFile()) assert !file.text.contains('com.google')
        } }
    }
    println("${edition} optional Gradle source/dependency/manifest isolation passed")
}
