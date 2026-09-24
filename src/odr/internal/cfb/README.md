# CFB implementation

CFB is the Microsoft Compound File Binary File Format. The reader is own code
in `cfb_impl.*`.

## Features

- [x] from memory
- [x] from file
- [x] list entries
- [x] read as stream
- [ ] write as stream (`CfbArchive::save` throws `UnsupportedOperation`)

## References

- [[MS-CFB]: Compound File Binary File Format](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-cfb)
- [microsoft/compoundfilereader](https://github.com/microsoft/compoundfilereader)

### Related work

- https://developer.gnome.org/gsf/
- https://github.com/ironfede/openmcdf (c#)
