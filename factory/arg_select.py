class ArgSelectError(Exception): pass
class NoneFoundError(ArgSelectError): pass

def select(choices, prompt='Select one', choice_str_fn=str):
    if len(choices) == 0:
        raise NoneFoundError()
    elif len(choices) == 1:
        return choices[0]
    else:
        print(prompt)
        for i in range(len(choices)):
            print('  %2u: %s' % (i, choice_str_fn(choices[i])))
        return choices[int(input('>>'))]
