func main()
  var b : bool
  var n, i: int
  var a : char

  i = 4;
  read n;
  
  case i-2*n of
     12 : n = n+1;
          write n;

     10 : i = i*2-n;          

     2 :
          b = true;
          if not b or i>4 then
             n = 22;
             case a of
                'X' : b = true;
                'Y' : n = 0;
             endcase
          endif
             
     default:
          b = false;
          i  = 0;
  endcase
  
  a = b>0 or n/a;

endfunc
