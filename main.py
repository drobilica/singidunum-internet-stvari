import machine
import time
import network
import urequests

# ThingSpeak Settings
THINGSPEAK_WRITE_API_KEY = 'M5WNW7Y1Z41OKKJH'
THINGSPEAK_URL = 'https://api.thingspeak.com/update?api_key=' + THINGSPEAK_WRITE_API_KEY

# Connect to WiFi
def connect_to_wifi(ssid, password):
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(ssid, password)

    # Wait for connection
    max_attempts = 10
    start = time.time()
    while not wlan.isconnected() and (time.time() - start) < max_attempts:
        # Non-blocking wait
        pass

    if wlan.isconnected():
        print("Connected to WiFi")
    else:
        print("Failed to connect to WiFi")
        return False

    return True

# Send data to ThingSpeak
def send_to_thingspeak(data):
    response = urequests.get(THINGSPEAK_URL + data)
    print(response.text)
    response.close()

# Parse Arduino data
def parse_data(data_str):
    data_points = data_str.strip().split(',')
    data_to_send = ""
    for point in data_points:
        key, value = point.split(':')
        if key == 'Temperature':
            data_to_send += "&field1=" + value.strip()
        elif key == 'Distance':
            data_to_send += "&field2=" + value.strip()
        elif key == 'LUX':
            data_to_send += "&field3=" + value.strip()
    return data_to_send

# Main program
def main():
    if connect_to_wifi('drobilica', '2112paris'):
        uart = machine.UART(0, 9600)  # UART connection on GP4 (TX) and GP5 (RX)
        last_send_time = time.time()
        latest_data = None

        while True:
            if uart.any():
                data = uart.readline().decode('utf-8')
                latest_data = data  # Update latest data

            if time.time() - last_send_time >= 2 and latest_data is not None:
                print("Sending data:", latest_data)
                parsed_data = parse_data(latest_data)
                send_to_thingspeak(parsed_data)
                last_send_time = time.time()
                latest_data = None  # Reset latest data after sending

            time.sleep(0.1)  # Non-blocking wait

if __name__ == "__main__":
    main()


