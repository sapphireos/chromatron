# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2022  Jeremy Billheimer
# 
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# </license>

# Streaming multithreaded key-value service
# with fast lookup thread safe dictionary.
# A high throughput view of the entire Catbus network
# while using the existing Catbus protocol with 
# no modifications.


import time
import threading
from copy import deepcopy
import logging

from catbus.directory import Directory
from catbus.client import Client, BackgroundClient



class CatHerder(threading.Thread):
	def __init__(self):
		super().__init__()

		self.max_workers = 4
		self._workers = {}

		self._directory = Directory()
		
		self._request_keys = {'wifi_rssi'}

		self._lock = threading.Lock()
		self._stop_event = threading.Event()
		self._semaphore = threading.Semaphore(self.max_workers)

		self.daemon = True
		self.start()

	@property
	def directory(self):
		return self._directory.get_directory()

	@property
	def data(self):
		data = {}

		with self._lock:	
			for host, worker in self._workers.items():
				data[host] = worker.data

		return data

	def run(self):
		while not self._stop_event.is_set():
			time.sleep(1.0)
		
			for host_str, device in self.directory.items():
				host = device['host']

				if host_str in self._workers:
					continue

				self._workers[host_str] = BackgroundClient(host, semaphore=self._semaphore)
				logging.info(f'Starting worker: {host}')



			# prune
			remove = []
			for host_str, cat in self._workers.items():
				if host_str not in self.directory:
					remove.append(host_str)

			for host_str in remove:
				logging.info(f'Pruning worker: {host_str}')
				self._workers[host_str].stop()
				del self._workers[host_str]


			# update data
			with self._lock:
				self._kv_data = {}



		for worker in self._workers.values():
			worker.stop()

		for worker in self._workers.values():
			worker.join()

	def stop(self):
		self._stop_event.set()


if __name__ == "__main__":
	from sapphire.common.util import setup_basic_logging
	setup_basic_logging()

	from pprint import pprint


	c = CatHerder()

	# c = BackgroundClient(('10.0.0.211', 44632))

	while True:
		try:
			time.sleep(1.0)

			pprint(c.data)

		except KeyboardInterrupt:
			break

	c.stop()
	c.join()





