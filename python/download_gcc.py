
import sys
import urllib.request, urllib.parse, urllib.error

from sapphire.buildtools.core import *


def progress(chunk, chunk_size, total_size):
    percent = round(((float(chunk) * chunk_size) / total_size) * 100, 0)
    
    if percent > 100:
        percent = 100

    sys.stdout.write("\r%d%%" % (percent))
    sys.stdout.flush()

if __name__ == "__main__":
    for platform in list(build_tool_archives.values()):
        for tool in platform:

            print("Downloading: %s" % (tool[1]))

            urllib.request.URLopener().retrieve(tool[0] + tool[1], tool[1], reporthook=progress)

            print('')