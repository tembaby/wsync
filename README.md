This program synchronize local directory with remote directory heirarchy using FTP (ftplib), it can also replicate it to other local directory. wsync will then track changed files and uploads (synchronize) them without the need to re-upload the whole directory tree. It then keeps this information in a file in the local directory called .wsync.conf. It will keep these files remotely under the same directory name as the local one.

wsync track changes to files by modification time. When first run (by issuing: cd target_directory && wsync) it will ask the user for list of file names/patterns to exclude from uploading, and will ignore these files. Some files are ignore by default, like the mentioned .wsync.conf since it could contain sensitive information.
wsync will then allow the user to enter destination directory information like whether it is on local or ftp filesystem, active or passive FTP connections (to work behind firewalls), proxy information, FTP username and password, and optionally, a file name to load this information from. It saves this file read/write permissions to its owner only and never upload/replicate it to remote destination. wsync will never ask for this information again.

wsync is capable of generating HTML file containing listing of files (name and size) in a hyperlinked way (wsync -g) and saves it in a file named index.html. The below directory listing, all source code uploads in this page is maintained by wsync.

=== Target platforms:
* OpenBSD
* Solaris 8
* current version: 1.1.0

=== License: BSD
