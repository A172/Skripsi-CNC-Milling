; CEK ORIGIN X/Y - WAJIB TANPA MATA PAHAT
; Jalankan dari interface setelah Home All dan Pilih Origin G54-G59.3.
; File tidak mengubah atau menyimpan origin.
; Z tidak diperintah turun: spindle harus tetap pada posisi home Z.
; Mesin bergerak pelan ke X0 Y0 origin aktif, diam 10 detik, lalu Home All.

M5
G21
G90
M400
G0 X0 Y0 F600
M400
G4 P10000
G28
M5
