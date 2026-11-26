============================================================================
               Copyright (c) 2000-2005 SiS Technologies Inc.                                                                                       
                      SiS AGP Driver ReadMe.Txt           
                               for                                  
                      Win2000/XP/ME/98/Server2003                               

      This readme file is used to provide the information for installation,

       Install and Uninstall "SiS AGP Driver".    
============================================================================

    The SiS AGP driver is installed for AGP bus used by VGA Card.It offer efficient
    functions for VGA driver.With installation of AGP driver,it provide high 
    performance and good compatibility for different hardware.


  This package include four parts.
  1. WinXP/Win2000/Win2003 AGP filter driver: Which can support AGP 8x chipset if chipset and VGA card support. 
  2. WinME/Win98  AGP miniport driver:Which can support AGP 8x chipset if chipset and VGA card support.
  3. Some Windows patches:
     SIS_LIB.DLL,SISPORT.SYS (winxp) : they are used for reading PCI port.  
     SiSUSBrg.exe (winxp): Add registry for USB bug under WinXP 
     Mp_s3.exe (WinME):Patch old SiS 961 FERR bug
     Openhci.sys (Win98):Provided by Microsoft to update Win98 USB OHCI
  4. Execute ata133.exe for IDE to let IDE support UDMA 133 (XP) and 48bit Capacity of
     HD.It also set IDE device to run UDMA as possible.
  

                                CONTENTS
                        ***********************
1)  System Requirements
2)  Windows 2000/Windows XP/Windows 98/Windows ME/Windows 2003 Driver
3)  Package Version Identify
4)  Silent Install Driver
5)  Verifying Installation of the Driver



1) System Requirements

   * This package should be installed on the AGP chipset,and others' 
     chipset will not be supported.

   * The "SiS AGP Driver" supports the following Operation System.

        Operation System                Version
============================================================================
	Windows 2000			2195	
	Windows XP			5.1.2600.0 or above
        Windows Server 2003		5.2.3790.0 or above
        Millennium			RTM
        Windows 98                      4.10.1998
============================================================================


2)  Windows 2000/Windows XP/Windows 98/Windows ME/Windows 2003 Driver

    *************************
    ** Driver Installation **
    *************************
      Click the SETUP.EXE program and select "Install"  button  then it will
    install the "SiS AGP Driver" automatically on your Windows system. It 
    will copy the driver to the \WINNT\SYSTEM32\DRIVERS  and  set the related 
    registry tree.
   

    ***************************
    ** Driver Uninstallation **
    ***************************
      You  could remove the "SiS AGP Driver" by selecting the "Uninstall" 
      button. It will completely remove the driver from the sytsem.
     

3)  Package Version Identify

      The setup program will display the released data and version for this package.

4)  Silent Install Driver

    Run "setup -s" for WinXP/WIN2k/98/ME/2003.

5)  Verifying Installation of the Driver
     For Windows 2K/XP/98/Me/2003:
        1. Open "Control Panel".
        2. Select "Device Manager" from "Control Panel".
        3. Select  "System devices" from "Device Manager".
        4. Check if the "SiS Accelerated Graphics Port" exist or not.
        5. If it is then your installation is successful.

       