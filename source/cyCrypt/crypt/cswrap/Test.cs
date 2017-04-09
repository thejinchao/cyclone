using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;

using cyclone;

class Test
{
    static void Main(string[] args)
    {
        byte[] key = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
        Rijndael aes = new Rijndael(key);

        string plain_text = "And God called the light Day,  and the darkness he called Night.";
        byte[] input = System.Text.Encoding.Default.GetBytes(plain_text);
        int length = input.Length;
        byte[] encrypted =  {
                0xe7, 0x05, 0x0e, 0xdf, 0x2e, 0x5d, 0x97, 0x62, 0x36, 0xe9, 0x17, 0xb1, 0xc1, 0x73, 0xde, 0xca,
                0xa2, 0x4b, 0x50, 0x4c, 0x02, 0x49, 0xea, 0xbd, 0x26, 0x25, 0x76, 0x92, 0x7a, 0xcf, 0x68, 0xee,
                0xa7, 0xa6, 0xc3, 0x75, 0xa7, 0x32, 0x13, 0x74, 0x31, 0x0f, 0xa9, 0xca, 0x0e, 0x5e, 0xab, 0x99,
                0xc5, 0x31, 0xc0, 0xe4, 0x26, 0x9c, 0x26, 0x92, 0x1a, 0xf4, 0xd0, 0xd0, 0xef, 0xa8, 0x7b, 0x23 };

        byte[] output = new byte[length];
        byte[] output2 = new byte[length];
        byte[] temp = new byte[Rijndael.BLOCK_SIZE];

        aes.encrypt(input, length, ref output);
        Debug.Assert(output.SequenceEqual(encrypted));

        aes.decrypt(output, length, ref output2);
        Debug.Assert(System.Text.Encoding.Default.GetString(output2).CompareTo(plain_text) == 0);

        Array.Clear(output, 0, length);
        Array.Clear(output2, 0, length);

        byte[] iv = (byte[])Rijndael.DefaultIV.Clone();
        for (int i = 0; i < length; i += Rijndael.BLOCK_SIZE)
        {
            aes.encrypt(input.Skip(i).Take(Rijndael.BLOCK_SIZE).ToArray(), Rijndael.BLOCK_SIZE, ref temp, ref iv);
            Buffer.BlockCopy(temp, 0, output, i, Rijndael.BLOCK_SIZE);
        }
        Debug.Assert(output.SequenceEqual(encrypted));

        Array.Copy(Rijndael.DefaultIV, iv, Rijndael.DefaultIV.Length);
        for (int i = 0; i < length; i += Rijndael.BLOCK_SIZE)
        {
            aes.decrypt(output.Skip(i).Take(Rijndael.BLOCK_SIZE).ToArray(), Rijndael.BLOCK_SIZE, ref temp, ref iv);
            Buffer.BlockCopy(temp, 0, output2, i, Rijndael.BLOCK_SIZE);
        }
        Debug.Assert(System.Text.Encoding.Default.GetString(output2).CompareTo(plain_text) == 0);
    }
}
