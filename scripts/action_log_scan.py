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
        if i['action']['rtt'] == 11:
            #print("{}\t{}\t{}\t{}\t{}".format(i['index'], ss, i['action']['block_number'], i['action']['block_hash'], i['action']['authority'])) # just print info about blocks
            for j in i['action']['unit_uri_impacts']:
                if j['content_unit_uri'] == '44fYJECeAsapo2wAmrDWRq8Sc2XsHUQsgyznjEacv3RT': # the unit uri to count the views for
                    for k in j['views_per_channel']:
                        print("{}\t{}\t{}\t{}\t{}\t{}".format(i['index'], ss, i['action']['block_number'], i['action']['authority'], k['channel_address'], sv * k['view_count']))
    if not len(list):
        #work = 0 # enable this to stop the loop
        time.sleep(60) # alternatively can sleep for a minute and continue
    else:
        data_json['start_index']=list[-1]['index'] + 1

