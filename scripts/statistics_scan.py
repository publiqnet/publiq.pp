import sys, json, requests, time

work = 1

data_json = json.loads('{"rtt":54,"start_index":0,"max_count":10000}')

while work == 1:
    data = json.dumps(data_json)
    response = requests.post('http://127.0.0.1:14111/api', data=data) # set the preferred api endpoint
    list = response.json()['actions']

    for i in list:
        ss = "apply"
        sv = 1
        if i['logging_type'] == 'revert':
            ss = "revert"
            sv = -1
        if i['action']['rtt'] == 11: # BlockLog
            for j in i['action']['transactions']:
                if j['action']['rtt'] == 35: # ServiceStatistics
                    server_address = j['action']['server_address']
                    for file_item in j['action']['file_items']:
                        file_uri = file_item['file_uri']
                        unit_uri = file_item['unit_uri']
                        if file_uri == 'particular_file_uri_here':
                            for count_item in file_item['count_items']:
                                count = count_item['count']
                                peer_address = count_item['peer_address']
                                print("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}".format(i['index'], ss, i['action']['block_number'], server_address, peer_address, sv * count, unit_uri, file_uri))
    if not len(list):
        work = 0 # enable this to stop the loop
        #time.sleep(60) # alternatively can sleep for a minute and continue
    else:
        data_json['start_index']=list[-1]['index'] + 1

