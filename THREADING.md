# Threading Guidelines

- The GUI thread must remain responsive. Avoid blocking database or file I/O on the UI thread.
- Use `QThread` workers or `QtConcurrent`/`QFutureWatcher` for long-running operations such as song import, media scanning, JSON parsing and network I/O.
- Communicate between threads using queued signals and slots. Do not access `QWidget` objects outside the GUI thread.
- Provide a cancellation mechanism for worker threads. Workers should periodically check a cancel token and exit gracefully when requested.
- Ensure that any temporary files or network requests are cleaned up when a task is canceled.
