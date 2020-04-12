#     Plot Application for CMS test data
#     Copyright (C) 2020  Ameya Mohape 
#     mohapeameya@gmail.com
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
#     along with this program.  If not, see <https://www.gnu.org/licenses/>.

#!/usr/bin/python3

import matplotlib.pyplot as plt
import csv

threadsAtClient = []
requestsPerSec = []
responseTime = []

with open('./log.txt','r') as csvfile:
    plots = csv.reader(csvfile, delimiter=',')
    for row in plots:
        threadsAtClient.append(int(row[0]))
        requestsPerSec.append(float(row[1]))
        responseTime.append(float(row[2]))

plt.plot(threadsAtClient,requestsPerSec, label='Threads at client vs Requests per sec')
plt.xlabel('Threads at client')
plt.ylabel('Requests per sec')
plt.title('Title: Threads at client vs Requests per sec')
plt.legend()
plt.show()

plt.plot(threadsAtClient,responseTime, label='Threads at client vs Response ime')
plt.xlabel('Threads at client')
plt.ylabel('response time')
plt.title('Title: Threads at client vs Response time')
plt.legend()
plt.show()
