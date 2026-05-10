# maxrefdes42 iolink rtd temp sensor quick start iq2 rev0 2015 01

- Source PDF: `docs/source-pdfs/application-notes/maxrefdes42_iolink_rtd_temp_sensor_quick_start_iq2_rev0_2015-01.pdf`
- Extraction date: 2026-05-09
- Page count: 20
- SHA256: `27b7accd3dcd4e55526eb48caf8de02d1192880ec5db31bcdeed7973b056b463`

## Page 1

For
  pricing,
  delivery,
  and
  ordering
  information,
  please
  contact
  Maxim
  Direct
  at
  1-­‐888-­‐629-­‐4642,
  or
  visit
  Maxim
  Integrated’s
  website
  at
  www.maximintegrated.com.
   Maxim Integrated cannot assume responsibility for use of any circuitry other than circuitry entirely embodied in a Maxim Integrated product. No circuit patent licenses are implied. Maxim Integrated reserves the right to change the circuitry and specifications without notice at any time.  © 2015 Maxim Integrated Products, Inc.               Maxim Integrated and the Maxim Integrated logo are trademarks of Maxim Integrated Products, Inc.
   MAXREFDES42#  IO-Link RTD Temp Sensor Quick Start Guide  (IQ2 IO-Link Master Version)  Rev 0; 1/15

## Page 2

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
2
Table of Contents 1.
  Required Equipment ................................................................................................. 3
  2.
  Overview ................................................................................................................... 5
  3.
  Included Files ............................................................................................................ 6
  4.
  Procedure .................................................................................................................. 8
  5.   Trademarks .............................................................................................................. 19
  6.   Revision History ....................................................................................................... 20

## Page 3

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
3
1. Required Equipment • PC with Windows® 7 or Windows 8 (Verify with IQ2 Development that your version of Windows is supported before purchasing their software.) • MAXREFDES42# board • IQ2 Development iqInterface® IO-Link® master with corresponding USB and power cables (This must be purchased separately.) • IQ2 Development IO-Link iqTool® (tested with version 1.1.0.4 and comes with the iqInterface IO-Link master) • One IO-Link cable (This must be purchased separately.) • One A-to-B Type USB cable • RD42_RL78_V01_XX.ZIP (Maxim-MAXREFDES42-20140824-IODD1.1.html), where XX = minor version
 Figure 1. MAXREFDES42# Board Connected to  an IQ2 Development IO-Link Master

## Page 4

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
4
  Figure 2. Green Display is Lit

## Page 5

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
5
2. Overview 1. Connect power and all the cables to the iqInterface IO-Link Master as shown in Figure 1.  2. Connect the MAXREFDES42# proximity sensor board to the other side of the IO-Link cable. Make sure the green display is lit as shown in Figure 2.   3. Download the latest “all design files” RD42V01_XX.ZIP file located on the Design Resources tab at www.maximintegrated.com/MAXREFDES42. Alternatively, you can download the design files from  http://www.iq2-development.com/downloads/.  4. Install the IQ2 Development IO-Link iqTool onto your Windows PC by referring to IQ2 Development’s iqInterface User Manual located at  http://www.iq2-development.com/downloads/.  5. Connect the MAXREFDES42# IO-Link RTD temperature sensor as a device in the IQ2 Development IO-Link iqTool.  6. Read and write to the desired indexes by referring to IQ2 Development’s .html index definition file (Maxim-MAXREFDES42-20140824-IODD1.1.html).

## Page 6

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
6
3. Included Files The RD42_RL78_V01_XX.ZIP contains the corresponding IO-Link Device Descriptor (IODD) files. The IODD contains information on communication properties, device parameters, identification, process, and diagnostic data. It includes an XML file, an image of the device, an icon image, and the manufacturer’s logo. The IODD structure is the same for all devices of all manufacturers, and is always represented in the same way by the IODD interpreter tools. See Figures 3 to 6 for project structure and key filenames.
 Figure 3. Directories inside RD42_RL78_VXX_XX.ZIP.
 Figure 4. File inside the Code_Documentation directory.

## Page 7

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
7
 Figure 5. Files inside the Design_Files directory. Source files are in the zip file.
  Figure 6. Files inside the IODD directory. The .xml files are the IODD files and the .html files are the IQ2 Development’s .html index definition files.

## Page 8

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
8
4. Procedure 1. Connect the A-to-B Type USB cable from the PC to the iqInterface IO-Link Master as shown in Figure 1.   2. Connect an IO-Link cable to the iqInterface IO-Link Master as shown in Figure 1.  3. Connect 24V DC and GND to the orange terminal block on the iqInterface IO-Link Master as shown in Figure 1. Pin 1 is +24V and pin 2 is GND.  4. Connect the MAXREFDES42# proximity sensor board to the other side of the IO-Link cable. Make sure the green display is lit as shown in Figure 2.   5. Download the latest “all design files” RD42V01_XX.ZIP file located on the DESIGN RESOURCES tab at www.maximintegrated.com/MAXREFDES42. Alternatively, you can download the design files from  http://www.iq2-development.com/downloads/.  6. Extract the RD42V01_XX.ZIP file to a directory on your PC.

## Page 9

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
9
7. Install the IQ2 Development IO-Link iqTool onto your Windows PC as shown in Figure 7 by referring to IQ2 Development’s iqInterface User Manual located at http://www.iq2-development.com/downloads/.
  Figure 7. Newly installed IQ2 Development IO-Link iqTool.

## Page 10

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
10
8. Make sure you are in the iqMaster tab, then press the Setting button. In the IQ Connection Settings window, select auto in the Comport / USB drop-down menu. Press OK to save setting as shown in Figure 8.
  Figure 8. IQ Connection Settings window.

## Page 11

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
11
9. Next, press the Connect button as shown in Figure 9.
  Figure 9. Press the Connect button in the iqTool.

## Page 12

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
12
 10. If the COM port is not found automatically, open Device Manager in your Windows operating system as shown in Figure 10 to verify to which COM port the iqInterface hardware is connected. Then manually set that COM port in the IQ Connection Settings window and press the Connect button again. If you still cannot connect after doing this step, contact IQ2 Development’s technical support or refer to IQ2 Development iqInterface user manual located at http://www.iq2-development.com/downloads/.
  Figure 10. Device Manager built into Windows.

## Page 13

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
13
 11. After the iqTool has connected to the correct COM port, press the Auto button as shown in Figure 11.
  Figure 11. iqTool Auto button.

## Page 14

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
14
 12.  You should now see a flashing green Operate circle and the On-request Data Read Request group should have become active (ungrayed out) as shown in Figure 12.
  Figure 12. iqTool Operate state.

## Page 15

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
15
13. Open the Maxim-MAXREFDES42-20140824-IODD1.1.html file to view the index numbers and data formats. Go to the index 261, which is the MAX31865 RTD code register value variable as shown in Figure 13. Note: RTD code register value is shifted by 1 bit so the value read back needs to be divided by two.
 Figure 13. .html index definition file – Index 261.

## Page 16

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
16
14. Read the RTD code by performing the operations in the On-request Data Read Request group below, as shown in Figure 14. • Change Index type to uint16. • Enter 261 in the Index field. • Change the Data type to uint16. • Press the Read button. Note: RTD code register value is shifted by 1 bit so the value read back needs to be divided by two.  RTD = (Code * 400) / 2^15 = (18174 / 2) * 400/ 2^15 = 110.925 Ohms
 Figure 14. iqTool - On-request Data Read Request group – Index 261.

## Page 17

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
17
15. Next, go to index 268 in the Maxim-MAXREFDES42-20140824-IODD1.1.html file , which is the ambient temperature in degree C variable, as shown in Figure 15.
 Figure 15. .html index definition file – Index 268.

## Page 18

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
18
16. Read the ambient temperature value in degrees C by performing the operations in the On-request Data Read Request group below, as shown in Figure 16. • Enter 268 in the Index field. • Change the Data type to hex. • Press the Read button. • Convert read hex value to float32.  Temp = 0x41E00E8A = 28.007099 = 28.01 degrees C
 Figure 16. iqTool - On-request Data Read Request group – Index 268.

## Page 19

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
19
5. Trademarks IO-Link is a registered trademark of ifm electronic GmbH. IQ2 Development is a registered trademark of IQ2 Development GmbH. iqInterface is a registered trademark of IQ2 Development GmbH. iqTool is a registered trademark of IQ2 Development GmbH. Windows is a registered trademark and registered service mark of Microsoft Corp.

## Page 20

MAXREFDES42# IO-Link RTD Temp Sensor Quick Start Guide (IQ2 IO-Link Master Version)
20
6. Revision History REVISION NUMBER REVISION DATE DESCRIPTION PAGES CHANGED 0 1/15 Initial release —
