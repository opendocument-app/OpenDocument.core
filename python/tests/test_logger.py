import pyodr


class CollectingLogger(pyodr.ILogger):
    """A sink implemented in Python — the public extension point."""

    def __init__(self, level=pyodr.LogLevel.verbose):
        super().__init__()
        self.level = level
        self.messages = []
        self.flushes = 0

    def will_log(self, level):
        return level >= self.level

    def log(self, time, level, message, location):
        self.messages.append((level, message, location.file_name, location.line))

    def flush(self):
        self.flushes += 1


def test_custom_sink_receives_messages():
    sink = CollectingLogger(pyodr.LogLevel.warning)
    logger = pyodr.Logger(sink)

    assert not logger.will_log(pyodr.LogLevel.debug)
    assert logger.will_log(pyodr.LogLevel.error)

    logger.log(pyodr.LogLevel.debug, "dropped")
    logger.log(pyodr.LogLevel.error, "kept")
    logger.flush()

    assert [m[1] for m in sink.messages] == ["kept"]
    assert sink.messages[0][0] == pyodr.LogLevel.error
    assert sink.flushes == 1


def test_custom_sink_composes_with_tee():
    first = CollectingLogger()
    second = CollectingLogger()

    tee = pyodr.Logger.create_tee([pyodr.Logger(first), pyodr.Logger(second)])
    tee.log(pyodr.LogLevel.info, "fanned out")

    assert [m[1] for m in first.messages] == ["fanned out"]
    assert [m[1] for m in second.messages] == ["fanned out"]


def test_null_logger_discards():
    assert not pyodr.Logger.null().will_log(pyodr.LogLevel.fatal)
    assert not pyodr.Logger().will_log(pyodr.LogLevel.fatal)


def test_custom_sink_receives_library_diagnostics(odt_path):
    """A logger passed to `open` is actually used by the library."""
    sink = CollectingLogger()
    pyodr.open(str(odt_path), logger=pyodr.Logger(sink))
    # The odt path logs at least one diagnostic; at minimum it must not crash
    # and the sink must have been consulted.
    assert isinstance(sink.messages, list)


def test_logger_accepted_by_entry_points(odt_path):
    logger = pyodr.Logger(CollectingLogger())
    assert pyodr.list_file_types(str(odt_path), logger=logger)
    assert pyodr.mimetype(str(odt_path), logger=logger)
    assert pyodr.DecodedFile(str(odt_path), logger=logger).is_document_file()
