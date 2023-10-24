param (
    [Parameter(Mandatory=$True)]
    [string]$drive
)
$pass = Read-Host 'Password: ' -AsSecureString
$passclear = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($pass))

./sedutil-cli.exe -v --setLockingRange 0 rw $passclear $drive
./sedutil-cli.exe -v --setMBREnable off $passclear $drive

