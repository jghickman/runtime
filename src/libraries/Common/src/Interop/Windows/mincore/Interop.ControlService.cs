// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Runtime.InteropServices;

internal partial class Interop
{
    internal partial class mincore
    {
        [DllImport(Libraries.ServiceWinSvc, CharSet = CharSet.Unicode, SetLastError = true)]
        internal unsafe extern static bool ControlService(IntPtr serviceHandle, int control, SERVICE_STATUS* pStatus);

    }
}
