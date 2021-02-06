
import sys
import time
from catbus import CatbusService
from sapphire.common import util, run_all, Ribbon
import logging
import json
import requests
from pprint import pprint
import asyncio

class WeatherService(Ribbon):
    def __init__(self, settings={}):
        self.kv = CatbusService(name='weather', visible=True, tags=[])
        self.kv['station'] = settings['station']

        self.start()

    def _process(self):
        logging.info("Fetching weather")

        result = requests.get(f"https://api.weather.gov/stations/{self.kv['station']}/observations/latest")
        
        props = result.json()['properties']
        # pprint(props)

        try:
            self.kv['weather_temperature']          = float(props['temperature']['value'])
            self.kv['weather_wind_direction']       = float(props['windDirection']['value'])
            self.kv['weather_wind_speed']           = float(props['windSpeed']['value'])
            self.kv['weather_relative_humidity']    = float(props['relativeHumidity']['value'])
            self.kv['weather_pressure']             = float(props['barometricPressure']['value']) / 1000.0 # convert to kPa

            logging.info("Update successful")

        except TypeError:
            # sometimes the weather API returns nulls for some reason.
            logging.warn('api returned nulls')

        time.sleep(60.0)


def main():
    util.setup_basic_logging(console=True)

    settings = {"station": "KAUS"}
    try:
        with open('settings.json', 'r') as f:
            settings = json.loads(f.read())

    except FileNotFoundError:
        pass

    w = WeatherService(settings=settings)

    run_all()

    


