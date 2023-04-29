
func main()
  var b : bool
  var n, i: int
  var a : char
  var x : float  

  i = 4;
  read n;

  case not b or x>1 of
     12 : n = n+1;
          write n;

      6 : i = 0;
          case a of
             'X' : b = true;
             2.3 : x = x/3;
              0  : n = 0;
          endcase

      8 : b = true;
          while not x or i>4 do
             n = 22;
             case n-1 of
                0 : n = n+1;
                true : n = n-1;
                'X' : b = false;
             endcase
          endwhile
             
     default:
         b = true;
         x = false;
         i = x;
  endcase

endfunc
