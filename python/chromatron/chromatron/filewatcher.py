
import threading
import hashlib

class Watcher(threading.Thread):
    def __init__(self, filename):
        super(Watcher, self).__init__()

        self.filename = filename
        self._file_hash = self._get_hash()

        self.daemon = True

        self._event = threading.Event()
        self._stop_event = threading.Event()

        self.start()

    def _get_hash(self):
        m = hashlib.md5()
        m.update(open(self.filename).read())

        return m.hexdigest()

    def run(self):
        while not self._stop_event.is_set():
            new_hash = self._get_hash()

            if new_hash != self._file_hash:
                self._event.set()
                self._file_hash = new_hash

            self._stop_event.wait(0.5)

    def changed(self):
        is_changed = self._event.is_set()

        if is_changed:
            self._event.clear()

        return is_changed

    def stop(self):
        self._stop_event.set()