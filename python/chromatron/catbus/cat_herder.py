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
from copy import copy, deepcopy
import logging

from catbus.directory import Directory
from catbus.client import Client, BackgroundClient
from catbus import query_tags

class WorkQueueEmpty(Exception):
	pass

class Worker(threading.Thread):
	def __init__(self, herder=None):
		super().__init__()

		self._herder = herder

		self._stop_event = threading.Event()

		self.client = Client()

		self.daemon = True
		self.start()

	def stop(self):
		logging.info(f'Stopping worker')
		self._stop_event.set()

	def run(self):
		logging.info(f'Starting worker')

		while not self._stop_event.is_set():
			self._herder.wait_for_timer()

			request_keys = self._herder.request_keys

			kv_data = {}

			try:
				while True:
					host, host_key = self._herder.get_work_item()
				
					self.client.connect(host)
					kv = self.client.get_keys(*request_keys)

					kv_data[host_key] = kv

			except WorkQueueEmpty:
				pass

			self._herder.update_kv(kv_data)

		logging.info(f'Worker exit')


class CatHerder(threading.Thread):
	def __init__(self, workers=1, rate=1.0):
		super().__init__()

		self._max_workers = workers
		self._rate = rate

		self._query = []
		self._request_keys = []
		self._kv_data = {}

		self._directory = Directory()
		
		self._lock = threading.Lock()
		self._stop_event = threading.Event()
		self._timer_event = threading.Event()
		
		self._work_q = []
		self._workers = []

		self.daemon = True
		self.start()

	def stop(self):
		self._stop_event.set()

	def join(self):
		for worker in self._workers:
			worker.join()

		super().join()

	@property
	def query(self):
		with self._lock:
			return copy(self._query)

	@query.setter
	def query(self, value):
		with self._lock:
			self._query = copy(value)

	@property
	def request_keys(self):
		with self._lock:
			return copy(self._request_keys)

	@request_keys.setter
	def request_keys(self, value):
		with self._lock:
			self._request_keys = copy(value)	

	@property
	def data(self):
		with self._lock:
			return copy(self._kv_data)

	def update_kv(self, kv_data={}):
		directory = self.directory

		for k, v in kv_data.items():
			v.update(directory[k])

		with self._lock:
			self._kv_data.update(kv_data)

	def get_work_item(self):
		with self._lock:
			try:
				return self._work_q.pop(0)

			except IndexError:
				raise WorkQueueEmpty

	def wait_for_timer(self):
		self._timer_event.wait()

	@property
	def directory(self):
		return self._directory.get_directory()

	def run(self):
		logging.info(f'Starting Herder')

		self._workers = [Worker(self) for i in range(self._max_workers)]

		while not self._stop_event.is_set():
			time.sleep(self._rate)

			work_q = []

			with self._lock:
				for host_str, device in self.directory.items():
					if not query_tags(self._query, device['query']):
						continue

					host = device['host']

					work_q.append((host, host_str))

					# print(host, device['name'], device['query'], self._query)

				self._work_q = work_q

			self._timer_event.set()
			self._timer_event.clear()


		for worker in self._workers:
			worker.stop()

		# set timer event so workers will stop
		self._timer_event.set()

		logging.info(f'Herder exit')




# class CatHerder(threading.Thread):
# 	def __init__(self):
# 		super().__init__()

# 		self.max_workers = 4
# 		self._workers = {}

# 		self._directory = Directory()
		
# 		self._request_keys = {'wifi_rssi'}

# 		self._lock = threading.Lock()
# 		self._stop_event = threading.Event()
# 		self._semaphore = threading.Semaphore(self.max_workers)

# 		self.daemon = True
# 		self.start()

# 	@property
# 	def directory(self):
# 		return self._directory.get_directory()

# 	@property
# 	def data(self):
# 		data = {}

# 		with self._lock:	
# 			for host, worker in self._workers.items():
# 				data[host] = worker.data

# 		return data

# 	def run(self):
# 		while not self._stop_event.is_set():
# 			time.sleep(1.0)
		
# 			for host_str, device in self.directory.items():
# 				host = device['host']

# 				if host_str in self._workers:
# 					continue

# 				self._workers[host_str] = BackgroundClient(host, semaphore=self._semaphore)
# 				logging.info(f'Starting worker: {host}')


# 			for worker in self._workers.values():
# 				worker.set_keys({'kv_test_key': 123})

# 			# prune
# 			remove = []
# 			for host_str, cat in self._workers.items():
# 				if host_str not in self.directory:
# 					remove.append(host_str)

# 			for host_str in remove:
# 				logging.info(f'Pruning worker: {host_str}')
# 				self._workers[host_str].stop()
# 				del self._workers[host_str]


# 			# update data
# 			with self._lock:
# 				self._kv_data = {}



# 		for worker in self._workers.values():
# 			worker.stop()

# 		for worker in self._workers.values():
# 			worker.join()

# 	def stop(self):
# 		self._stop_event.set()


if __name__ == "__main__":
	from sapphire.common.util import setup_basic_logging
	setup_basic_logging()

	from pprint import pprint

	c = CatHerder()

	c.query = ['living_room']
	c.request_keys = ['wifi_rssi', 'gfx_max_dimmer', 'gfx_sub_dimmer']


	while True:
		try:
			time.sleep(1.0)

			pprint(c.data)

		except KeyboardInterrupt:
			break

	c.stop()
	c.join()





