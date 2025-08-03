# ARTDynInsnsPatchDemo

A demo to show dynamic instruction patch.

## Building

- Build and get the APK

```cmd
./gradlew assembleRelease
```

> You can also use `assembleDebug`, but release mode is recommended cuz there's usually only 1 dex.

- Use [DexHollower](https://github.com/RinOfficial0615/DexHollower) to get `code_item.bin`.

- Use MT Manager or other tools to replace `classes.dex` and `assets/code_item.bin` with the new ones.

- Install the APK and test it.

## Further Reading

It's recommended to read [this](docs/art_method_code_patching.md) for implementation details.

## Acknowledgements

- [DJI - The ART of obfuscation](https://blog.quarkslab.com/dji-the-art-of-obfuscation.html)
- [LSPlant](https://github.com/LSPosed/LSPlant)
- [Dobby](https://github.com/jmpews/Dobby)
- [xDL](https://github.com/hexhacking/xDL)
- [Android Code Search](https://cs.android.com/)
