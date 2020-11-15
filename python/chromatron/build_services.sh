pyinstaller logserver.spec
cp dist/logserver docker/logserver

pyinstaller timezone_service.spec
cp dist/timezone_service docker/timezone_service

pyinstaller weather.spec
cp dist/weather docker/weather