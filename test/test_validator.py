def validate(result1,result2):
    print("Validating %s" % result1.name)
    
    return (result1.appid == result2.appid)


def cleaner(result):
    print("Cleaning %s" % result.name)

    return True
