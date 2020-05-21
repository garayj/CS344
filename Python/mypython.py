import string
import random

# Only print lowercase strings.
letters = string.ascii_lowercase

for i in range(3):
    # Create a string and print it.
    randomString = ''.join(random.choice(letters) for i in range(10))
    print(randomString)

    # Open file to write to with the same name as the string
    f = open(randomString + ".txt", "w")
    f.write(randomString + "\n")
    f.close()

randMulti = 1
for i in range(2):
    # Create a new number.
    randNum = random.randint(1, 42)
    # Multuply and print
    randMulti = randMulti * randNum
    print(randNum)

print(randMulti)
