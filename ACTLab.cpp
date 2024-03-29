// Include the ACTLab header file where the class methods
// and properties are declared (and defined), as well as
// any required libraries.

#include <ACTLab.h>

// ======================================================================
// =================================================== Constructor method

ACTLabClass::ACTLabClass () {
	// Set private properties (with default values).
	MAC(0x90,0xA2,0xDA,0x00,0x7F,0xAB);	// Adeeb's E.S.'s MAC address.
	_SDBuffer = 0;						// Default = 0, incase SD card not present.
	server(31,170,160,87);				// 000webhost.com server.
	_HTTP = 1;							// Default = 1, (i.e. POST) since it's more secure.
	_SDPin = 4;							// SD CS for Ethernet Shield.
	_serial = 0;						// Default = 0, since only really used for troubleshooting.
}

// ======================================================================
// ================================================ Configuration Methods

// ACTLab.rig()

void ACTLabClass::rig (String rig) {
	rig.toCharArray(_rig,16);
}

// ACTLab.MAC()

void ACTLabClass::MAC (byte b0,byte b1,byte b2,byte b3,byte b4,byte b5) {
	_MAC[0] = b0;
	_MAC[1] = b1;
	_MAC[2] = b2;
	_MAC[3] = b3;
	_MAC[4] = b4;
	_MAC[5] = b5;
}

// ACTLab.SDBuffer()

void ACTLabClass::SDBuffer (int arg) {
	if (arg==0||arg==1) {_SDBuffer = arg;};
}

// ACTLab.server()

void ACTLabClass::server (byte b0,byte b1,byte b2,byte b3) {
	_server[0] = b0;
	_server[1] = b1;
	_server[2] = b2;
	_server[3] = b3;
}

// ACTLab.HTTP()

void ACTLabClass::HTTP (int arg) {
	if (arg==0||arg==1) {_HTTP = arg;};
}

// ACTLab.SDPin()

void ACTLabClass::SDPin (int arg) {
	_SDPin = arg;
}

// ACTLab.serialMessages()

void ACTLabClass::serialMessages (int arg) {
	if (arg==0||arg==1) {_serial = arg;};
}

// ======================================================================
// ================================================ Core Ethernet Methods
	
// ACTLab.start()
	
void ACTLabClass::start () {
	// Start the ethernet shield.
	_serialPrintln("Starting ethernet shield.");
	if (Ethernet.begin(_MAC)) {
		_serialPrintln("Ethernet shield started.");
	} else {
		_serialPrint("Failed to start ethernet shield. ");
		_serialPrintln("Is the ethernet cable plugged in?");
	};
	
	// Also initialize SD library and card if requested. Clear SDBuffer.txt as well.
	if (_SDBuffer == 1) {
		_serialPrintln("Initializing SD library and card.");
		//pinMode(10, OUTPUT); // Needed? Arduino
		//pinMode(53, OUTPUT); // Needed? Mega
		if (SD.begin(_SDPin)) {
			_serialPrintln("SD library and card initialized.");
			SDBuffer_clear();
		} else {
			_serialPrintln("Failed to start SD library and card. ");
			_serialPrintln("Is there a SD card in the shield?");
		};
	};
}

// ACTLab.checkForInstruction()

bool ACTLabClass::checkForInstruction () {
	// Initialize a static (persistant) EthernetClient object, called client.
	static EthernetClient client;		
	
	// Variable declarations.
	bool rtn = false;
	bool whileBool = true;
	int status_experiment = 0;		// 0 = Not started. 1 = Started. 2 = Ended.
	int status_parameters = 0;		// 0 = Not started. 1 = Started. 2 = Ended.
	char experimentBuffer[8] = {};
	char parametersBuffer[128] = {};
	char *p_pos_1;
	char *p_pos_2 = parametersBuffer;
	char *p_pos_3;
	char *p_param;
	int _parametersPos = 0;
	
	// Connect to server.
	_serialPrintln("Connecting to server (2).");
	if (client.connect(_server, 80)) { // Port 80 is the default for HTTP.
		_serialPrintln("Connected to server (2).");
		
		// Build up the http request's parameters.
		char paramsBuffer[60] = {}; // Nice conservative buffer size.
		strcat(paramsBuffer,"rig=");
		strcat(paramsBuffer,_rig);
		strcat(paramsBuffer,"&action=check");
		strcat(paramsBuffer,"\0");
		String paramsEncoded = paramsBuffer;
		paramsEncoded.replace("+","%2B");
		paramsEncoded.replace("-","%2D");
		
		// Decide whether to send a GET or POST HTTP request.
		if (_HTTP==0) {
			// Send a HTTP GET request.
			client.print("GET ");
			client.print("http://actlab.comli.com/application/scripts/instruction.php?");
			client.print(paramsEncoded);
			client.println(" HTTP/1.0");
			client.println();
		} else {
			// Send a HTTP POST request - Default (should be anyway).
			client.print("POST ");
			client.print("http://actlab.comli.com/application/scripts/instruction.php");
			client.println(" HTTP/1.0");
			client.println("Content-Type: application/x-www-form-urlencoded");
			client.print("Content-Length: ");
			client.println(paramsEncoded.length());
			client.println();
			client.print(paramsEncoded); // ! - Does this need to be encoded?
			client.println();
		};
		
		while(whileBool){
			// Check if there are incomming bytes from the server.
			if (client.available()) {
				// Get the next character.
				char c = client.read();
				
				// Check if the character is not a bracket.
				if (c!='{'&&c!='}'&&c!='['&&c!=']') {
					// Upgrade c char to string in a temp variable.
					char temp[2] = {c,'\0'};
					
					// If reading experiment.
					if (status_experiment==1) {strcat(experimentBuffer,temp);};
					// If reading parameters.
					if (status_parameters==1) {strcat(parametersBuffer,temp);};
				};
				
				// If c = experiment bracket start.
				if (c=='{') {status_experiment=1;};
				// If c = experiment bracket end.
				if (c=='}') {status_experiment=2;};
				// If c = parameters bracket start.
				if (c=='[') {status_parameters=1;};
				// If c = parameters bracket end.
				if (c==']') {status_parameters=2;rtn = true;};
			};
			
			// If the server has disconnected, stop the while loop.
			if (!client.connected()) {whileBool = false;};
		};
		
		// If instructions successfully recieved, process them.
		if (rtn) {
			// Convert the experiment number from string to double (then to int via equality).
			_experiment = strtod(experimentBuffer,&p_pos_1);
			
			// Clear the _parameters array.
			for (int i = 0; i < 10; i++) {_parameters[i] = NULL;};
			
			// Process the parameters to doubles.
			while ((p_param = strtok_r(p_pos_2, ",", &p_pos_2)) != NULL) { // Delimiter: comma.
				_parameters[_parametersPos] = strtod(p_param,&p_pos_3);
				_parametersPos ++;
			};
		};
		
		// Disconnect from server.
		client.stop();
		client.flush();
		_serialPrintln("Disconnected from server (2).");
		
		// If instructions successfully recieved, let server know.
		if (rtn) {
			// Build up the http request's parameters.
			char paramsBuffer[60] = {}; // Nice conservative buffer size.
			strcat(paramsBuffer,"rig=");
			strcat(paramsBuffer,_rig);
			strcat(paramsBuffer,"&action=received");
			strcat(paramsBuffer,"\0");
			
			// Pass on data to _ethernetClient for submission.
			_ethernetClient("http://actlab.comli.com/application/scripts/instruction.php",paramsBuffer);	
			
		};
	}
	// Could not connect to server.
	else {_serialPrintln("Connection to server failed (2).");};
	
	// Return outcome.
	return rtn;
}

// ACTLab.getExperimentNumber()

int ACTLabClass::getExperimentNumber () {
	return _experiment;
}

// ACTLab.getParameter()

double ACTLabClass::getParameter (int number) {
	return _parameters[number];
}

// ACTLab.submitData()

void ACTLabClass::submitData (double time, double reference, double input, double output) {
	// Declare the buffers for the four parameters.
	char param_time[12];
	char param_reference[12];
	char param_input[12];
	char param_output[12];
	
	// Convert the four parameters from doubles to an ASCII representation (/string).
	dtostre(time,param_time,3,0);
	dtostre(reference,param_reference,3,0);
	dtostre(input,param_input,3,0);
	dtostre(output,param_output,3,0);
	
	// Build up the http request's parameters.
	char paramsBuffer[60] = {}; // Nice conservative buffer size.
	strcat(paramsBuffer,"rig=");
	strcat(paramsBuffer,_rig);
	strcat(paramsBuffer,"&t=");
	strcat(paramsBuffer,param_time);
	strcat(paramsBuffer,"&r=");
	strcat(paramsBuffer,param_reference);
	strcat(paramsBuffer,"&i=");
	strcat(paramsBuffer,param_input);
	strcat(paramsBuffer,"&o=");
	strcat(paramsBuffer,param_output);
	strcat(paramsBuffer,"\0");
	
	// Serial output the parameters in their various forms for info.
	_serialPrint("time = ");_serialPrintln(param_time);
	_serialPrint("reference = ");_serialPrintln(param_reference);
	_serialPrint("input = ");_serialPrintln(param_input);
	_serialPrint("output = ");_serialPrintln(param_output);
	_serialPrint("paramsBuffer = ");_serialPrintln(paramsBuffer);
	
	// Pass on data to _ethernetClient for submission.
	_ethernetClient("http://actlab.comli.com/application/scripts/recordData.php",paramsBuffer);
}

// ======================================================================
// ==================================================== SD Buffer Methods

// ACTLab.SDBuffer_clear()

void ACTLabClass::SDBuffer_clear () {
	// Delete SDBuffer file if exists.
	if (SD.exists("SDBuffer.txt")) {
		SD.remove("SDBuffer.txt");
		_serialPrintln("SDBuffer.txt cleared (really deleted).");
	};
}

// ACTLab.SDBuffer_add()

void ACTLabClass::SDBuffer_add (double time, double reference, double input, double output) {
	// Create File object.
	File fileObject;
	
	// Declare the buffers for the four parameters.
	char param_time[12];
	char param_reference[12];
	char param_input[12];
	char param_output[12];
	
	// Convert the four parameters from doubles to an ASCII representation (/string).
	dtostre(time,param_time,3,0);
	dtostre(reference,param_reference,3,0);
	dtostre(input,param_input,3,0);
	dtostre(output,param_output,3,0);
	
	// Build up the line to add to the text file.
	char paramsBuffer[60] = {}; // Nice conservative buffer size.
	strcat(paramsBuffer,param_time);
	strcat(paramsBuffer,";");
	strcat(paramsBuffer,param_reference);
	strcat(paramsBuffer,";");
	strcat(paramsBuffer,param_input);
	strcat(paramsBuffer,";");
	strcat(paramsBuffer,param_output);
	
	fileObject = SD.open("SDBuffer.txt", FILE_WRITE);
	if (fileObject) {
		fileObject.println(paramsBuffer);
		fileObject.close();
	};
}

// ACTLab.SDBuffer_submit()

void ACTLabClass::SDBuffer_submit () {
	// Variables
	int newln = 1;		// Is the current character the first in a new line.
	int param = 0;		// Which parameter is currently being read.
						// 		0 = Time.
						// 		1 = Reference.
						// 		2 = Input.
						// 		3 = Output.
	int dataSets = 0;		// How many data sets have been read so far.
	int dataSetsLim = 10;	// Max. num. of data sets to be submitted in one go.
	char paramsBuffer[600] = {};	// ~ dataSetsLim * 60.
	strcat(paramsBuffer,"rig=");
	strcat(paramsBuffer,_rig);
	char temp[2];
	int temp2;
						
	// Create File object.
	File fileObject;
	
	// Open SDBuffer.txt into object.
	fileObject = SD.open("SDBuffer.txt");
	if (fileObject) {
		// Initiate while loop to read each character in the file, one at a time.
		while (fileObject.available()) {
			// Get the character.
			char c = char(fileObject.read());
			
			// If new line started.
			if (newln == 1) {
				param = 0;
				strcat(paramsBuffer,"&t");
				memset(temp,0,2);temp2=(dataSets+1);temp[0]=((char)temp2);temp[1]='\0';
				strcat(paramsBuffer,temp);
				strcat(paramsBuffer,"=");
				newln = 0;
			};
			
			// If character is a normal one.
			if ((((char)c)!=';')&&(c!='\n')) {
				memset(temp,0,2);temp[0]=((char)c);temp[1]='\0';
				strcat(paramsBuffer,temp);
			}
			// If character is a semi-colon.
			else if (((char)c) == ';') {
				if (param == 0) {
					strcat(paramsBuffer,"&r");
					memset(temp,0,2);temp2=(dataSets+1);temp[0]=((char)temp2);temp[1]='\0';
					if (dataSets!=0) {strcat(paramsBuffer,temp);};
					strcat(paramsBuffer,"=");
				}
				else if (param == 1) {
					strcat(paramsBuffer,"&i");
					memset(temp,0,2);temp2=(dataSets+1);temp[0]=((char)temp2);temp[1]='\0';
					if (dataSets!=0) {strcat(paramsBuffer,temp);};
					strcat(paramsBuffer,"=");
				}
				else if (param == 2) {
					strcat(paramsBuffer,"&o");
					memset(temp,0,2);temp2=(dataSets+1);temp[0]=((char)temp2);temp[1]='\0';
					if (dataSets!=0) {strcat(paramsBuffer,temp);};
					strcat(paramsBuffer,"=");
				};
				param ++;
			}
			// If character is a new line.
			else if (c == '\n') {
				newln = 1;
				dataSets ++;
				if (dataSets == (dataSetsLim-1)) {
					_serialPrintln("");
					_serialPrintln(paramsBuffer);
					_serialPrintln("");
					memset(paramsBuffer,0,600);
					strcat(paramsBuffer,"rig=");
					strcat(paramsBuffer,_rig);
					dataSets = 0;
				};
			};
		};
		
		// Close the file:
		fileObject.close();
	} else {
		// If the file didn't open.
		_serialPrintln("Error opening SDBuffer.txt.");
	};
}

// ======================================================================
// ====================================================== Private Methods

// _ethernetClient()

void ACTLabClass::_ethernetClient (char url[], char params[]) {
	// Initialize a static (persistant) EthernetClient object, called client.
	static EthernetClient client;
	
	// Encode the parameters.
	String paramsEncoded = params;
	paramsEncoded.replace("+","%2B");
	paramsEncoded.replace("-","%2D");
	
	// Serial print the encoded parameters for info.
	_serialPrint("paramsEncoded = ");if (_serial) {Serial.println(paramsEncoded);};
	
	// Connect to server.
	_serialPrintln("Connecting to server.");
	if (client.connect(_server, 80)) { // Port 80 is the default for HTTP.
		_serialPrintln("Connected to server.");
		
		// Decide whether to send a GET or POST HTTP request.
		if (_HTTP==0) {
			// Send a HTTP GET request.
			client.print("GET ");
			client.print(url);
			client.print("?");
			client.print(paramsEncoded);
			client.println(" HTTP/1.0");
			client.println();
		} else {
			// Send a HTTP POST request - Default (should be anyway).
			client.print("POST ");
			client.print(url);
			client.println(" HTTP/1.0");
			client.println("Content-Type: application/x-www-form-urlencoded");
			client.print("Content-Length: ");
			client.println(paramsEncoded.length());
			client.println();
			client.print(paramsEncoded); // ! - Does this need to be encoded?
			client.println();
		};
		
		// Disconnect from server.
		client.stop();
		client.flush();
		_serialPrintln("Disconnecting from server.");
	}
	// Could not connect to server.
	else {_serialPrintln("Connection to server failed.");};
}

// _serialPrint()

void ACTLabClass::_serialPrint (char str[]) {
	if (_serial) {Serial.print(str);};
}

// _serialPrintln()

void ACTLabClass::_serialPrintln (char str[]) {
	if (_serial) {Serial.println(str);};
}

// Initialize an ACTLab object.

ACTLabClass ACTLab;