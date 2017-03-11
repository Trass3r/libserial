
int main(int argc, const char** argv)
{
	Serial serial(port, baudrate);
	serial.setErrorHandler([](const char* msg) { std::cerr << "ERROR: " << msg << std::endl; });
	serial.setTimeout(100);
	if (!serial.open())
		return 1;

	const uint32_t bufferSize = 32;
	uint8_t data[bufferSize] = {};

	BufferedSerial bufserial(serial);
	while (true)
	{
		size_t bytesRead = bufserial.read(data, bufferSize);
		if (printData(&data[0], bytesRead), true)
			continue;
	}

	return 0;
}
