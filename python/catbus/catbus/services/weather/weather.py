
import sys
import time
from catbus import CatbusService
from sapphire.common import util, wait_for_signal, Ribbon

import json
import requests
from pprint import pprint


class WeatherService(Ribbon):
    def initialize(self, settings={}):
        self.name = 'weather'
        self.station = settings['station']
        
        self.kv = CatbusService(name='weather', visible=True, tags=[])

        self.kv['station'] = self.station

        self.kv.send('weather_temperature',         'weather_temperature', ['datalog'])
        self.kv.send('weather_wind_direction',      'weather_wind_direction', ['datalog'])
        self.kv.send('weather_wind_speed',          'weather_wind_speed', ['datalog'])
        self.kv.send('weather_relative_humidity',   'weather_relative_humidity', ['datalog'])
        self.kv.send('weather_pressure',            'weather_pressure', ['datalog'])

    def loop(self):
        result = requests.get(f"https://api.weather.gov/stations/{self.station}/observations/latest").json()
        
        props = result['properties']
        # pprint(props)

        self.kv['weather_temperature']          = float(props['temperature']['value'])
        self.kv['weather_wind_direction']       = float(props['windDirection']['value'])
        self.kv['weather_wind_speed']           = float(props['windSpeed']['value'])
        self.kv['weather_relative_humidity']    = float(props['relativeHumidity']['value'])
        self.kv['weather_pressure']             = float(props['barometricPressure']['value']) / 1000.0 # convert to kPa

        self.wait(60.0)

    def clean_up(self):
        self.kv.stop()


def main():
    util.setup_basic_logging(console=True)

    settings = {"station": "KAUS"}
    try:
        with open('settings.json', 'r') as f:
            settings = json.loads(f.read())

    except FileNotFoundError:
        pass

    w = WeatherService(settings=settings)

    wait_for_signal()

    w.stop()
    w.join()

    


