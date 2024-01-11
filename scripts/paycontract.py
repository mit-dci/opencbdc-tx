def pay(pay, balance1, balance2):
    balance1 = int(balance1)
    balance2 = int(balance2)
    pay = int(pay)
    if(pay > balance1):
        pay = 0
    balance1 = balance1 - pay
    balance2 = balance2 + pay
    return [balance1, balance2]

def accrueInterest(rate, balance1):
    balance1 = int(balance1)
    rate = float(rate)
    print("Balance 1:", balance1)
    print("Rate:", rate*100, "%")
    balance1 = int(balance1 * (2.71828**rate))
    print("Balance 1:", balance1)
    return [balance1]

def pay2(pay1, pay2, balance0, balance1, balance2):
    balance0 = int(balance0)
    balance1 = int(balance1)
    balance2 = int(balance2)
    pay1 = int(pay1)
    pay2 = int(pay2)
    if(pay1 + pay2 > balance0 and pay1 > 0 and pay2 > 0):
        pay1 = 0
        pay2 = 0
    balance0 = balance0 - pay1 - pay2
    balance1 = balance1 + pay1
    balance2 = balance2 + pay2
    return [balance0, balance1, balance2]

