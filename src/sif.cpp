// #include "sif.h"
// #include "common.h"

Sif sif = {0};

void 
sif_reset()
{
	syslog("Resetting SIF\n");
	memset(&sif, 0, sizeof(Sif));
}

void
sif_write(u32 address, u32 value) 
{
	switch(address)
	{
		/*************
			EE Base
		*************/ 
		case 0x1000F200:
			sif.mscom = value;
			syslog("EE->IOP Communication\n");
			syslog("SIF_WRITE: MSCOM. Value [{:#08x}]\n", value);
		break;
		
		case 0x1000F220: 
			sif.msflg = value;
			syslog("SIF_WRITE: MSFLG. Value [{:#08x}]\n", value);
		break;

		case 0x1000F240: 
			sif.ctrl = value;
			syslog("SIF_WRITE: CTRL. Value [{:#08x}]\n", value);
		break;

		case 0x1000F260: 
			sif.bd6 = value;
			syslog("SIF_WRITE: BD6. Value [{:#08x}]\n", value);
		break;

		/*************
			IOP Base
		*************/ 
		default:
			errlog("[ERROR]: Unrecognized address from sif_write [{:#08x}]\n", address);
		break;
	}

	return;
}

u32 
sif_read(u32 address) 
{
	switch(address)
	{
		case 0x1000F200:
			syslog("SIF_READ: MSCOM\n");
			return sif.mscom;
		break; 
		
		case 0x1000F220: 
			syslog("SIF_READ: MSFLG\n");
			return sif.msflg;
		break;

		case 0x1000F230: 
			syslog("SIF_READ: SMFLG.\n");
			return sif.smflg;
		break;

		default:
			errlog("[ERROR]: Unrecognized address from sif_write [{:#08x}]\n", address);
			return 0;
		break;
	}
}