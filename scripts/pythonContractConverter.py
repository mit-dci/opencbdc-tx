# assumes all return types are strings
def parseContract(file, funcName):
    import re

    fin = -1
    try:
        fin = open(file)
    except:
        print("[ERROR] {fname} cannot be opened".format(fname = file))
        return ""

    data = fin.read()
    fin.close()
    pattern = re.compile(r'#.*?\n')
    data = re.sub(pattern, '', data)
    pattern = re.compile(r"'''.*?'''", re.DOTALL)
    data = re.sub(pattern, '', data)
    if(("def " + funcName + "(") not in data):
        print("[ERROR] {fname} does not contain a definition for \"{func}\"".format(fname = file, func = funcName))
        return ""
    data = data.replace('\n', '\n|')
    data_lines = data.split('|')

    idx = 0
    for i in range(len(data_lines)):
        if("def " + funcName + "(" in data_lines[i]):
            idx = i
            break
    data_lines = data_lines[idx:]


    while(True):
        try:
            data_lines.remove('\n')
        except:
            break

    tabLength = 0
    tabChar = False
    for i in data_lines[1]:
        if(i == '\t'):
            tabChar = True
            tabLength = 1
            break
        elif(i == ' '):
            tabLength += 1
        else:
            break

    startChar = '\t' if tabChar else ' ' * tabLength
    lastElem = 1
    for i in range(1, len(data_lines)+1):
        if(i == len(data_lines) or len(data_lines[i]) < tabLength or data_lines[i][0:tabLength] != startChar):
            lastElem = i
            break
        else:
            data_lines[i] = data_lines[i][tabLength:]
    data_lines = data_lines[0:lastElem]

    # Get the return parameters of the function
    return_params = ""
    idx = -1
    for i in range(len(data_lines)):
        if("return" in data_lines[i]):
            replacement_line = ""
            return_params = data_lines[i]
            idx = i
            return_params = return_params.split(",")
            if("[" in return_params[0]):
                return_params[0] = return_params[0].split("[")[-1]
            if("]" in return_params[-1]):
                return_params[-1] = return_params[-1].split("]")[0]
            for j in range(len(return_params)):
                return_params[j] = return_params[j].strip()
                replacement_line += return_params[j] + " = str(" + return_params[j] + ")\n"
                return_params[j] += ","
            data_lines[i] = replacement_line

    # TODO: Update this to handle different data types
    d_types = 's' * len(return_params)

    # Get the input args to the function
    line = ""
    idx = -1
    for i in range(len(data_lines)):
        if(("def " + funcName + "(") in data_lines[i]):
            line = data_lines[i]
            idx = i
            break
    line = line.split(",")
    line[0] = line[0].split("(")[-1]
    line[-1] = line[-1].split(")")[0]
    for i in range(len(line)):
        line[i] = line[i].strip()
        line[i] += ","

    data_lines[idx] = ""
    for elem in return_params:
        data_lines[idx] += elem
    data_lines[idx] += '|'
    for elem in line:
        data_lines[idx] += elem
    data_lines[idx] += '|'


    contract = d_types + '|'

    for elem in data_lines:
        contract += elem

    return contract


contract = parseContract(file, funcname)
# print(contract)
