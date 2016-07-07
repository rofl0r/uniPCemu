Program Highres256;
uses dos,crt,x86EMU;

var
  id: string;
  regs: registers;
  x,y: integer;
  color: byte;
  status: boolean;
  params: array[1..2] of byte;
  result: array[1..2] of byte;
  paramlength: word;
  resultlength: word;

begin
  id := getDebuggerID; (* get the debugger id! *)
  regs.AH := 0;
  regs.AL := $2E;
  intr($10,regs); (* switch to 640x480x256 mode *)

  status := resetDebugger(id); (* Try and reset the debugger! *)
  if (status) then (* Debugger ready? *)
  begin
    params[1] := 1; (* enable *)
    paramlength := 1; (* 1 byte param *)
    resultlength := 1; (* One byte result *)
    status := debuggerSendCommand(id,$00,$03,@params,paramlength,@result,@resultlength); (* Start logging VGA accesses *)
  end;

  for x := 0 to 639 do
    for y := 0 to 479 do
    begin
      regs.AH := $C;
      regs.AL := color;
      regs.BH := 0;
      regs.CX := x;
      regs.DX := y;
      intr($10,regs); (* Put a pixel! *)
    end;

  if (status) then
  begin
    params[1] := 0; (* disable *)
    paramlength := 1; (* 1 byte param *)
    resultlength := 1; (* One byte result *)
    status := debuggerSendCommand(id,$00,$03,@params,paramlength,@result,@resultlength); (* Start logging VGA accesses *)
  end;

  regs.AH := 0;
  regs.AL := 2;
  intr($10,regs); (* switch back to text mode *)
end.