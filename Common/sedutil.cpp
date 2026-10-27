/* C:B**************************************************************************
This software is Copyright 2014-2017 Bright Plaza Inc. <drivetrust@drivetrust.com>

This file is part of sedutil.

sedutil is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

sedutil is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sedutil.  If not, see <http://www.gnu.org/licenses/>.

* C:E********************************************************************** */
#include <iostream>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include "os.h"
#include "DtaHashPwd.h"
#include "DtaOptions.h"
#include "DtaLexicon.h"
#include "DtaDevGeneric.h"
#include "DtaDevOpal1.h"
#include "DtaDevOpal2.h"
#include "DtaDevEnterprise.h"

using namespace std;

/* Default to output that omits timestamps and goes to stdout */
sedutiloutput outputFormat = sedutilReadable;

int isValidSEDDisk(char *devname)
{
	DtaDev * d;
	d = new DtaDevGeneric(devname);
	if (d->isPresent()) {
		printf("%s", devname);
		if (d->isAnySSC())
			printf(" SED %s%s%s ", (d->isOpal1() ? "1" : "-"),
			(d->isOpal2() ? "2" : "-"), (d->isEprise() ? "E" : "-"));
		else
			printf("%s", " NO --- ");
		cout << d->getModelNum() << " " << d->getFirmwareRev();
		cout << std::endl;
	}
	delete d;
	return 0;
}

/* A password argument of "-" is read as one line from stdin instead, so it
 * does not show up in ps or /proc/<pid>/cmdline. Lines are consumed in
 * argument order (password first, then newpassword). On a terminal it prompts
 * with echo off. A literal "-" password can still be given as -x 2d. */
static struct termios savedTermios;

/* restore echo if the password prompt is interrupted, then die from the signal */
static void restoreTermiosAndRaise(int sig)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &savedTermios);
	signal(sig, SIG_DFL);
	raise(sig);
}

static char * readPasswordFromStdin(char * buf, size_t size, const char * prompt)
{
	static const int signals[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGTSTP };
	struct sigaction restore = {}, previous[sizeof(signals) / sizeof(signals[0])];
	struct termios noecho;
	bool tty = isatty(STDIN_FILENO) && (0 == tcgetattr(STDIN_FILENO, &savedTermios));
	if (tty) {
		restore.sa_handler = restoreTermiosAndRaise;
		for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
			sigaction(signals[i], &restore, &previous[i]);
		fprintf(stderr, "%s: ", prompt);
		noecho = savedTermios;
		noecho.c_lflag &= ~ECHO;
		/* TCSANOW, not TCSAFLUSH: keep input typed or pasted ahead for this prompt */
		tcsetattr(STDIN_FILENO, TCSANOW, &noecho);
	}
	char * line = fgets(buf, (int)size, stdin);
	if (tty) {
		tcsetattr(STDIN_FILENO, TCSANOW, &savedTermios);
		for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
			sigaction(signals[i], &previous[i], NULL);
		fprintf(stderr, "\n");
	}
	if (NULL == line) {
		LOG(E) << "Unable to read password from stdin";
		return NULL;
	}
	size_t len = strcspn(buf, "\r\n");
	if (('\0' == buf[len]) && (len == size - 1)) {
		/* no line end within the buffer: refuse instead of using a truncated
		 * password and leaving the rest for the next read */
		memset(buf, 0, size);
		LOG(E) << "Password read from stdin is longer than " << size - 2 << " characters";
		return NULL;
	}
	buf[len] = '\0';
	return buf;
}

int main(int argc, char * argv[])
{
	DTA_OPTIONS opts;
	DtaDev *tempDev = NULL, *d = NULL;
	static char stdinPassword[256], stdinNewPassword[256];
	if (DtaOptions(argc, argv, &opts)) {
		return DTAERROR_COMMAND_ERROR;
	}
	if (opts.password && !strcmp(argv[opts.password], "-")) {
		if (NULL == (argv[opts.password] = readPasswordFromStdin(stdinPassword, sizeof(stdinPassword), "Password")))
			return DTAERROR_COMMAND_ERROR;
	}
	if (opts.newpassword && !strcmp(argv[opts.newpassword], "-")) {
		if (NULL == (argv[opts.newpassword] = readPasswordFromStdin(stdinNewPassword, sizeof(stdinNewPassword), "New password")))
			return DTAERROR_COMMAND_ERROR;
	}
	
	if ((opts.action != sedutiloption::scan) && 
		(opts.action != sedutiloption::validatePBKDF2) &&
		(opts.action != sedutiloption::isValidSED)) {
		if (opts.device > (argc - 1)) opts.device = 0;
		tempDev = new DtaDevGeneric(argv[opts.device]);
		if (NULL == tempDev) {
			LOG(E) << "Create device object failed";
			return DTAERROR_OBJECT_CREATE_FAILED;
		}
		if ((!tempDev->isPresent()) || (!tempDev->isAnySSC())) {
			LOG(E) << "Invalid or unsupported disk " << argv[opts.device];
			delete tempDev;
			return DTAERROR_COMMAND_ERROR;
		}
		if (tempDev->isOpal2())
			d = new DtaDevOpal2(argv[opts.device]);
		else
			if (tempDev->isOpal1())
				d = new DtaDevOpal1(argv[opts.device]);
			else
				if (tempDev->isEprise())
					d = new DtaDevEnterprise(argv[opts.device]);
				else
				{
					LOG(E) << "Unknown OPAL SSC ";
					return DTAERROR_INVALID_COMMAND;
				}
		delete tempDev;
		if (NULL == d) {
			LOG(E) << "Create device object failed";
			return DTAERROR_OBJECT_CREATE_FAILED;
		}
		// make sure DtaDev::no_hash_passwords is initialized
		d->no_hash_passwords = opts.no_hash_passwords;

		d->hex_passwords = opts.hex_passwords;

		d->output_format = opts.output_format;
	}

    switch (opts.action) {
 	case sedutiloption::initialSetup:
		LOG(D) << "Performing initial setup to use sedutil on drive " << argv[opts.device];
        return (d->initialSetup(argv[opts.password]));
	case sedutiloption::setup_SUM:
		LOG(D) << "Performing SUM setup on drive " << argv[opts.device];
		return (d->setup_SUM(opts.lockingrange, atoll(argv[opts.lrstart]),
			atoll(argv[opts.lrlength]), argv[opts.password], argv[opts.newpassword]));
		break;
	case sedutiloption::setSIDPassword:
        LOG(D) << "Performing setSIDPassword ";
        return d->setSIDPassword(argv[opts.password], argv[opts.newpassword]);
		break;
	case sedutiloption::setAdmin1Pwd:
        LOG(D) << "Performing setPAdmin1Pwd ";
        return d->setPassword(argv[opts.password], (char *) "Admin1",
                            argv[opts.newpassword]);
		break;
	case sedutiloption::loadPBAimage:
        LOG(D) << "Loading PBA image " << argv[opts.pbafile] << " to " << opts.device;
        return d->loadPBA(argv[opts.password], argv[opts.pbafile]);
		break;
	case sedutiloption::setLockingRange:
        LOG(D) << "Setting Locking Range " << (uint16_t) opts.lockingrange << " " << (uint16_t) opts.lockingstate;
        return d->setLockingRange(opts.lockingrange, opts.lockingstate, argv[opts.password]);
		break;
	case sedutiloption::setLockingRange_SUM:
		LOG(D) << "Setting Locking Range " << (uint16_t)opts.lockingrange << " " << (uint16_t)opts.lockingstate << " in Single User Mode";
		return d->setLockingRange_SUM(opts.lockingrange, opts.lockingstate, argv[opts.password]);
		break;
	case sedutiloption::enableLockingRange:
        LOG(D) << "Enabling Locking Range " << (uint16_t) opts.lockingrange;
        return (d->configureLockingRange(opts.lockingrange,
			(DTA_READLOCKINGENABLED | DTA_WRITELOCKINGENABLED), argv[opts.password]));
        break;
	case sedutiloption::disableLockingRange:
		LOG(D) << "Disabling Locking Range " << (uint16_t) opts.lockingrange;
		return (d->configureLockingRange(opts.lockingrange, DTA_DISABLELOCKING,
			argv[opts.password]));
		break;
	case sedutiloption::readonlyLockingRange:
		LOG(D) << "Enabling Locking Range " << (uint16_t)opts.lockingrange;
		return (d->configureLockingRange(opts.lockingrange,
			DTA_WRITELOCKINGENABLED, argv[opts.password]));
		break;
	case sedutiloption::setupLockingRange:
		LOG(D) << "Setup Locking Range " << (uint16_t)opts.lockingrange;
		return (d->setupLockingRange(opts.lockingrange, atoll(argv[opts.lrstart]),
			atoll(argv[opts.lrlength]), argv[opts.password]));
		break;
	case sedutiloption::setupLockingRange_SUM:
		LOG(D) << "Setup Locking Range " << (uint16_t)opts.lockingrange << " in Single User Mode";
		return (d->setupLockingRange_SUM(opts.lockingrange, atoll(argv[opts.lrstart]),
			atoll(argv[opts.lrlength]), argv[opts.password]));
		break;
	case sedutiloption::listLockingRanges:
		LOG(D) << "List Locking Ranges ";
		return (d->listLockingRanges(argv[opts.password], -1));
		break;
	case sedutiloption::listLockingRange:
		LOG(D) << "List Locking Range[" << opts.lockingrange << "]";
		return (d->listLockingRanges(argv[opts.password], opts.lockingrange));
		break;
    case sedutiloption::rekeyLockingRange:
		LOG(D) << "Rekey Locking Range[" << opts.lockingrange << "]";
		return (d->rekeyLockingRange(opts.lockingrange, argv[opts.password]));
        break;
    case sedutiloption::setBandsEnabled:
        LOG(D) << "Set bands Enabled";
        return (d->setBandsEnabled(-1, argv[opts.password]));
        break;
    case sedutiloption::setBandEnabled:
        LOG(D) << "Set band[" << opts.lockingrange << "] enabled";
        return (d->setBandsEnabled(opts.lockingrange, argv[opts.password]));
        break;
	case sedutiloption::setMBRDone:
		LOG(D) << "Setting MBRDone " << (uint16_t)opts.mbrstate;
		return (d->setMBRDone(opts.mbrstate, argv[opts.password]));
		break;
	case sedutiloption::setMBREnable:
		LOG(D) << "Setting MBREnable " << (uint16_t)opts.mbrstate;
		return (d->setMBREnable(opts.mbrstate, argv[opts.password]));
		break;
	case sedutiloption::enableuser:
        LOG(D) << "Performing enable user for user " << argv[opts.userid];
        return d->enableUser(argv[opts.password], argv[opts.userid]);
        break;
	case sedutiloption::activateLockingSP:
		LOG(D) << "Activating the LockingSP on" << argv[opts.device];
        return d->activateLockingSP(argv[opts.password]);
        break;
	case sedutiloption::activateLockingSP_SUM:
		LOG(D) << "Activating the LockingSP on" << argv[opts.device];
		return d->activateLockingSP_SUM(opts.lockingrange, argv[opts.password]);
		break;
	case sedutiloption::eraseLockingRange_SUM:
		LOG(D) << "Erasing LockingRange " << opts.lockingrange << " on" << argv[opts.device];
		return d->eraseLockingRange_SUM(opts.lockingrange, argv[opts.password]);
		break;
    case sedutiloption::query:
		LOG(D) << "Performing diskquery() on " << argv[opts.device];
        d->puke();
        return 0;
        break;
	case sedutiloption::scan:
        LOG(D) << "Performing diskScan() ";
        return(DtaDevOS::diskScan());
        break;
	case sedutiloption::isValidSED:
		LOG(D) << "Verify whether " << argv[opts.device] << "is valid SED or not";
        return isValidSEDDisk(argv[opts.device]);
        break;
	case sedutiloption::takeOwnership:
		LOG(D) << "Taking Ownership of the drive at" << argv[opts.device];
        return d->takeOwnership(argv[opts.password]);
        break;
 	case sedutiloption::revertLockingSP:
		LOG(D) << "Performing revertLockingSP on " << argv[opts.device];
        return d->revertLockingSP(argv[opts.password], 0);
        break;
	case sedutiloption::setPassword:
        LOG(D) << "Performing setPassword for user " << argv[opts.userid];
        return d->setPassword(argv[opts.password], argv[opts.userid],
                              argv[opts.newpassword]);
        break;
	case sedutiloption::setPassword_SUM:
		LOG(D) << "Performing setPassword in SUM mode for user " << argv[opts.userid];
		return d->setNewPassword_SUM(argv[opts.password], argv[opts.userid],
			argv[opts.newpassword]);
		break;
	case sedutiloption::revertTPer:
		LOG(D) << "Performing revertTPer on " << argv[opts.device];
        return d->revertTPer(argv[opts.password], 0, 0);
        break;
	case sedutiloption::revertNoErase:
		LOG(D) << "Performing revertLockingSP  keep global locking range on " << argv[opts.device];
		return d->revertLockingSP(argv[opts.password], 1);
		break;
	case sedutiloption::validatePBKDF2:
        LOG(D) << "Performing PBKDF2 validation ";
        TestPBKDF2();
        break;
	case sedutiloption::yesIreallywanttoERASEALLmydatausingthePSID:
	case sedutiloption::PSIDrevert:
		LOG(D) << "Performing a PSID Revert on " << argv[opts.device] << " with password " << argv[opts.password];
        return d->revertTPer(argv[opts.password], 1, 0);
        break;
	case sedutiloption::PSIDrevertAdminSP:
		LOG(D) << "Performing a PSID RevertAdminSP on " << argv[opts.device] << " with password " << argv[opts.password];
        return d->revertTPer(argv[opts.password], 1, 1);
        break;
	case sedutiloption::eraseLockingRange:
		LOG(D) << "Erase Locking Range " << (uint16_t)opts.lockingrange;
		return (d->eraseLockingRange(opts.lockingrange, argv[opts.password]));
		break;
	case sedutiloption::objDump:
		LOG(D) << "Performing objDump " ;
		return d->objDump(argv[argc - 5], argv[argc - 4], argv[argc - 3], argv[argc - 2]);
		break;
    case sedutiloption::printDefaultPassword:
		LOG(D) << "print default password";
        return d->printDefaultPassword();
        break;
    case sedutiloption::printPasswordHash:
		LOG(D) << "print password hash";
        return d->printPasswordHash(argv[opts.password]);
        break;
	case sedutiloption::prepareForS3Sleep:
        LOG(D) << "Preparing for S3 sleep " << (uint16_t) opts.lockingrange;
        return d->prepareForS3Sleep(opts.lockingrange, argv[opts.password]);
		break;
	case sedutiloption::rawCmd:
		LOG(D) << "Performing cmdDump ";
		return d->rawCmd(argv[argc - 7], argv[argc - 6], argv[argc - 5], argv[argc - 4], argv[argc - 3], argv[argc - 2]);
		break;
    default:
        LOG(E) << "Unable to determine what you want to do ";
        usage();
    }
	return DTAERROR_INVALID_COMMAND;
}
