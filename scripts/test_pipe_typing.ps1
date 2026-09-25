$typeDefinition = @"
using System;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct RECT {
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IpcKeyRequest {
    public uint MsgType;
    public uint VkCode;
    public uint IsKeyDown;
    public RECT CaretRect;
    public ulong Hwnd;
}

[StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Unicode)]
public struct IpcKeyResponse {
    public uint Eaten;
    public uint IsComposing;
    public uint CommitLen;
    public uint IsAsciiMode;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
    public string CommitText;
}
"@
Add-Type -TypeDefinition $typeDefinition

function Send-IpcKey([uint32]$vk, [bool]$isDown, [int]$left, [int]$top, [int]$right, [int]$bottom) {
    $pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "WeaselFluentNamedPipe", [System.IO.Pipes.PipeDirection]::InOut)
    $pipe.Connect(1000)

    $req = New-Object IpcKeyRequest
    $req.MsgType = 1 # IPC_MSG_KEY
    $req.VkCode = $vk
    $req.IsKeyDown = if ($isDown) { 1 } else { 0 }
    $req.CaretRect.Left = $left
    $req.CaretRect.Top = $top
    $req.CaretRect.Right = $right
    $req.CaretRect.Bottom = $bottom

    $sizeReq = [System.Runtime.InteropServices.Marshal]::SizeOf($req)
    $bytesReq = New-Object byte[] $sizeReq
    $ptr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($sizeReq)
    [System.Runtime.InteropServices.Marshal]::StructureToPtr($req, $ptr, $false)
    [System.Runtime.InteropServices.Marshal]::Copy($ptr, $bytesReq, 0, $sizeReq)
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)

    $pipe.Write($bytesReq, 0, $sizeReq)
    $pipe.Flush()

    $respDummy = New-Object IpcKeyResponse
    $sizeResp = [System.Runtime.InteropServices.Marshal]::SizeOf($respDummy)
    $bytesResp = New-Object byte[] $sizeResp
    $read = $pipe.Read($bytesResp, 0, $sizeResp)

    $ptrResp = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($sizeResp)
    [System.Runtime.InteropServices.Marshal]::Copy($bytesResp, 0, $ptrResp, $sizeResp)
    $resp = [System.Runtime.InteropServices.Marshal]::PtrToStructure($ptrResp, [Type][IpcKeyResponse])
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptrResp)

    $pipe.Close()
    return $resp
}

Write-Output "Sending 'N'..."
$r1 = Send-IpcKey 0x4E $true 400 300 402 320
Write-Output "Eaten: $($r1.Eaten), Composing: $($r1.IsComposing), Commit: '$($r1.CommitText)'"

Write-Output "Sending 'I'..."
$r2 = Send-IpcKey 0x49 $true 400 300 402 320
Write-Output "Eaten: $($r2.Eaten), Composing: $($r2.IsComposing), Commit: '$($r2.CommitText)'"

Write-Output "Sending Space (commit)..."
$r3 = Send-IpcKey 0x20 $true 400 300 402 320
Write-Output "Eaten: $($r3.Eaten), Composing: $($r3.IsComposing), Commit: '$($r3.CommitText)'"
