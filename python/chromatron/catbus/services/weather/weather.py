
import sys
import time
from catbus import CatbusService
from sapphire.common import util, run_all, Ribbon
import logging
import json
import requests
from pprint import pprint

class WeatherService(Ribbon):
    def __init__(self, settings={}):
        super().__init__()

        self.kv = CatbusService(name='weather', visible=True, tags=[])
        self.kv['station'] = settings['station']

        self.start()

    def _process(self):
        logging.info("Fetching weather")

        result = requests.get(f"https://api.weather.gov/stations/{self.kv['station']}/observations/latest")
        
        props = result.json()['properties']
        # pprint(props)

        for kv, source in [('temperature', 'temperature'), 
                           ('wind_direction', 'windDirection'),
                           ('wind_speed', 'windSpeed'),
                           ('relative_humidity', 'relativeHumidity'),
                           ('pressure', 'barometricPressure')]:

            try:
                key = 'weather_' + kv
                if key == 'pressure':
                    # convert to kPa
                    self.kv[key]          = float(props[source]['value']) / 1000.0

                else:
                    self.kv[key]          = float(props[source]['value'])

                logging.info("Update successful")

            except TypeError:
                # sometimes the weather API returns nulls for some reason.
                logging.warning(f'api returned nulls for {source}')

        self.wait(60.0)


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

    


