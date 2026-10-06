filename = input("Enter input file name: ")
with open(filename, "r") as f:
    while(1):
        a = f.readline()
        if not a:
            break
        a = a.strip()
        if not a:
            continue
        a = hex(int(a,2))
        print(a[2:], end=" ")