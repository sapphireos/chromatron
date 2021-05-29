
a = Number()
# b = Number()
# c = Number()

# def init():
#     a = '2021-08-19T03:45:59'
#     a = time()

#     b = cron(hour=3, minute=15)
#     c = cron(hour=4)


# def should_run():
#     return time() > cron(hour=3) and time() < cron(hour=4)

def should_run2():
    return within(cron(hour=3), cron(hour=4))

# @event(should_run)
def my_func():
	a = cron(hour=3)

    # if should_run2():
        # db.gfx_master_dimmer = 65535
