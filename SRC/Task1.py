def line_position(sensors):
    weights = [-2, -1, 0, 1, 2] 
    total = sum(sensors)
    if total == 0: # Sum zero means that none of the sensors are detecting the color of the path/line
        return None          # When line is not detected
    centroid = 0
    for i in range(5): # Calculate weighted Avg
        centroid += sensors[i]*weights[i]
    return centroid / total

print(line_position([0, 0, 0, 0, 0]))        # Output: None
print(line_position([0, 100, 100, 0, 0]))    # Output: -0.5 (line slightly left)
print(line_position([0, 0, 0, 0, 200]))      # Output: 2.0 (line far right)

def limit_motors(v,max):
    max = abs(max)   # if max value is negative
    if v > max:
        return max
    if v < -max:
        return -max
    return v

print(limit_motors(100,200),limit_motors(-300,200),limit_motors(300,200))
# Gives us 100, -200, 200

