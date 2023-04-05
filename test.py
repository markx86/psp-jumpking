#!/bin/python3

def sigmasum1(rep):
    return float((int(rep) * (int(rep) + 1))) * 0.5

def sigmasumf(n, i0, s):
    num = 0.0
    for i in range(int(i0), int(n + 1)):
        num += float(i) * float(s)
    return num

def calculatejumpstrength(height, step):
    num = 0.0
    num2 = 0.0
    num3 = 0
    while num < float(height):
        num2 += float(step)
        num += num2
        num3 += 1
    return num3 * float(step)

if __name__ == '__main__':
    fps30_jumpheight = sigmasum1(17.0)
    print("FPS30 Jump Height", fps30_jumpheight)
    gravity = fps30_jumpheight / sigmasumf(17.0 / 0.5, 1.0, 1.0)
    print("Gravity", gravity)
    jump = calculatejumpstrength(fps30_jumpheight, gravity)
    print("Jump", jump)
