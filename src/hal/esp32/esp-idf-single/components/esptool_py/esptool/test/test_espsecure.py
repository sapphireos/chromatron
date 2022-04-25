#!/usr/bin/env python
#
# Tests for espsecure.py
#
# Assumes openssl binary is in the PATH
from __future__ import division, print_function

import binascii
import io
import os
import os.path
import subprocess
import sys
import tempfile
import unittest
from collections import namedtuple

TEST_DIR = os.path.abspath(os.path.dirname(__file__))
os.chdir(TEST_DIR)

try:
    import espsecure
except ImportError:
    sys.path.insert(0, os.path.join(TEST_DIR, ".."))
    import espsecure

import esptool

from test_esptool import ESPSECURE_PY


class EspSecureTestCase(unittest.TestCase):

    def run_espsecure(self, args):
        """ Run espsecure.py with the specified arguments

        Returns output as a string if there is any, raises an exception if espsecure.py fails
        """
        cmd = [sys.executable, ESPSECURE_PY] + args.split(" ")
        print("Running %s..." % (" ".join(cmd)))

        try:
            output = subprocess.check_output([str(s) for s in cmd],
                                             cwd=TEST_DIR,
                                             stderr=subprocess.STDOUT)
            print(output)
            return output.decode("utf-8")
        except subprocess.CalledProcessError as e:
            print(e.output)
            raise e

    def setUp(self):
        self.cleanup_files = []  # keep a list of files _open()ed by each test case

    def tearDown(self):
        for f in self.cleanup_files:
            f.close()

    def _open(self, image_file):
        f = open(os.path.join('secure_images', image_file), 'rb')
        self.cleanup_files.append(f)
        return f


class ESP32SecureBootloaderTests(EspSecureTestCase):

    def test_digest_bootloader(self):
        DBArgs = namedtuple('digest_bootloader_args', [
            'keyfile',
            'output',
            'iv',
            'image'])

        try:
            output_file = tempfile.NamedTemporaryFile(delete=False)
            output_file.close()

            args = DBArgs(self._open('256bit_key.bin'),
                          output_file.name,
                          self._open('256bit_iv.bin'),
                          self._open('bootloader.bin'))
            espsecure.digest_secure_bootloader(args)

            with open(output_file.name, 'rb') as of:
                with self._open('bootloader_digested.bin') as ef:
                    self.assertEqual(ef.read(), of.read())
        finally:
            os.unlink(output_file.name)

    def test_digest_rsa_public_key(self):
        DigestRSAArgs = namedtuple('digest_rsa_public_key_args', [
            'keyfile',
            'output'])

        try:
            output_file = tempfile.NamedTemporaryFile(delete=False)
            output_file.close()

            args = DigestRSAArgs(self._open('rsa_secure_boot_signing_key.pem'),
                                 output_file.name)
            espsecure.digest_rsa_public_key(args)

            with open(output_file.name, 'rb') as of:
                with self._open('rsa_public_key_digest.bin') as ef:
                    self.assertEqual(ef.read(), of.read())
        finally:
            os.unlink(output_file.name)


class SigningTests(EspSecureTestCase):

    VerifyArgs = namedtuple('verify_signature_args', [
        'version',
        'keyfile',
        'datafile'])

    SignArgs = namedtuple('sign_data_args', [
        'version',
        'keyfile',
        'output',
        'append_signatures',
        'datafile'])

    ExtractKeyArgs = namedtuple('extract_public_key_args', [
        'version',
        'keyfile',
        'public_keyfile'])

    GenerateKeyArgs = namedtuple('generate_key_args', [
        'version',
        'keyfile'])

    def _test_sign_v1_data(self, key_name):
        try:
            output_file = tempfile.NamedTemporaryFile(delete=False)
            output_file.close()

            # Note: signing bootloader is not actually needed
            # for ESP32, it's just a handy file to sign
            args = self.SignArgs('1', [self._open(key_name)],
                                 output_file.name, None,
                                 self._open('bootloader.bin'))
            espsecure.sign_data(args)

            with open(output_file.name, 'rb') as of:
                with self._open('bootloader_signed.bin') as ef:
                    self.assertEqual(ef.read(), of.read())

        finally:
            os.unlink(output_file.name)

    def test_sign_v1_data(self):
        self._test_sign_v1_data('ecdsa_secure_boot_signing_key.pem')

    def test_sign_v1_data_pkcs8(self):
        self._test_sign_v1_data('ecdsa_secure_boot_signing_key_pkcs8.pem')

    def test_sign_v2_data(self):
        with tempfile.NamedTemporaryFile() as output_file:
            args = self.SignArgs('2', [self._open('rsa_secure_boot_signing_key.pem')],
                                 output_file.name, False,
                                 self._open('bootloader_unsigned_v2.bin'))
            espsecure.sign_data(args)

            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key.pem'),
                                   output_file)
            espsecure.verify_signature(args)

    def test_sign_v2_multiple_keys(self):
        # 3 keys + Verify with 3rd key
        with tempfile.NamedTemporaryFile() as output_file:
            args = self.SignArgs('2', [self._open('rsa_secure_boot_signing_key.pem'),
                                       self._open('rsa_secure_boot_signing_key2.pem'),
                                       self._open('rsa_secure_boot_signing_key3.pem')],
                                 output_file.name, False,
                                 self._open('bootloader_unsigned_v2.bin'))
            espsecure.sign_data(args)

            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key3.pem'),
                                   output_file)
            espsecure.verify_signature(args)

            output_file.seek(0)
            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key2.pem'),
                                   output_file)
            espsecure.verify_signature(args)

            output_file.seek(0)
            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key.pem'),
                                   output_file)
            espsecure.verify_signature(args)

    def test_sign_v2_append_signatures(self):
        # Append signatures + Verify with an appended key (bootloader_signed_v2.bin already signed with rsa_secure_boot_signing_key.pem)
        with tempfile.NamedTemporaryFile() as output_file:
            args = self.SignArgs('2', [self._open('rsa_secure_boot_signing_key2.pem'),
                                       self._open('rsa_secure_boot_signing_key3.pem')],
                                 output_file.name, True,
                                 self._open('bootloader_signed_v2.bin'))
            espsecure.sign_data(args)

            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key.pem'),
                                   output_file)
            espsecure.verify_signature(args)

            output_file.seek(0)
            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key2.pem'),
                                   output_file)
            espsecure.verify_signature(args)

            output_file.seek(0)
            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key3.pem'),
                                   output_file)
            espsecure.verify_signature(args)

    def test_sign_v2_append_signatures_multiple_steps(self):
        # similar to previous test, but sign in two invocations
        with tempfile.NamedTemporaryFile() as output_file1, tempfile.NamedTemporaryFile() as output_file2:
            args = self.SignArgs('2', [self._open('rsa_secure_boot_signing_key2.pem')],
                                 output_file1.name, True,
                                 self._open('bootloader_signed_v2.bin'))
            espsecure.sign_data(args)

            args = self.SignArgs('2', [self._open('rsa_secure_boot_signing_key3.pem')],
                                 output_file2.name, True,
                                 output_file1)
            espsecure.sign_data(args)

            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key.pem'),
                                   output_file2)
            espsecure.verify_signature(args)

            output_file2.seek(0)
            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key2.pem'),
                                   output_file2)
            espsecure.verify_signature(args)

            output_file2.seek(0)
            args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key3.pem'),
                                   output_file2)
            espsecure.verify_signature(args)

    def test_verify_signature_signing_key(self):
        # correct key v1
        args = self.VerifyArgs('1', self._open('ecdsa_secure_boot_signing_key.pem'),
                               self._open('bootloader_signed.bin'))
        espsecure.verify_signature(args)

        # correct key v2
        args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key.pem'),
                               self._open('bootloader_signed_v2.bin'))
        espsecure.verify_signature(args)

        # wrong key v1
        args = self.VerifyArgs('1', self._open('ecdsa_secure_boot_signing_key2.pem'),
                               self._open('bootloader_signed.bin'))
        with self.assertRaises(esptool.FatalError) as cm:
            espsecure.verify_signature(args)
        self.assertIn("Signature is not valid", str(cm.exception))

        # wrong key v2
        args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key2.pem'),
                               self._open('bootloader_signed_v2.bin'))
        with self.assertRaises(esptool.FatalError) as cm:
            espsecure.verify_signature(args)
        self.assertIn("Signature could not be verified with the provided key.", str(cm.exception))

        # multi-signed wrong key v2
        args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_key4.pem'),
                               self._open('bootloader_multi_signed_v2.bin'))
        with self.assertRaises(esptool.FatalError) as cm:
            espsecure.verify_signature(args)
        self.assertIn("Signature could not be verified with the provided key.", str(cm.exception))

    def test_verify_signature_public_key(self):
        # correct key v1
        args = self.VerifyArgs('1', self._open('ecdsa_secure_boot_signing_pubkey.pem'),
                               self._open('bootloader_signed.bin'))
        espsecure.verify_signature(args)

        # correct key v2
        args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_pubkey.pem'),
                               self._open('bootloader_signed_v2.bin'))
        espsecure.verify_signature(args)

        # wrong key v1
        args = self.VerifyArgs('1', self._open('ecdsa_secure_boot_signing_pubkey2.pem'),
                               self._open('bootloader_signed.bin'))
        with self.assertRaises(esptool.FatalError) as cm:
            espsecure.verify_signature(args)
        self.assertIn("Signature is not valid", str(cm.exception))

        # wrong key v2
        args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_pubkey2.pem'),
                               self._open('bootloader_signed_v2.bin'))
        with self.assertRaises(esptool.FatalError) as cm:
            espsecure.verify_signature(args)
        self.assertIn("Signature could not be verified with the provided key.", str(cm.exception))

        # multi-signed wrong key v2
        args = self.VerifyArgs('2', self._open('rsa_secure_boot_signing_pubkey4.pem'),
                               self._open('bootloader_multi_signed_v2.bin'))
        with self.assertRaises(esptool.FatalError) as cm:
            espsecure.verify_signature(args)
        self.assertIn("Signature could not be verified with the provided key.", str(cm.exception))

    def test_extract_binary_public_key(self):

        with tempfile.NamedTemporaryFile() as pub_keyfile, tempfile.NamedTemporaryFile() as pub_keyfile2:
            args = self.ExtractKeyArgs('1', self._open('ecdsa_secure_boot_signing_key.pem'),
                                       pub_keyfile)
            espsecure.extract_public_key(args)

            args = self.ExtractKeyArgs('1', self._open('ecdsa_secure_boot_signing_key2.pem'),
                                       pub_keyfile2)
            espsecure.extract_public_key(args)

            pub_keyfile.seek(0)
            pub_keyfile2.seek(0)

            # use correct extracted public key to verify
            args = self.VerifyArgs('1', pub_keyfile, self._open('bootloader_signed.bin'))
            espsecure.verify_signature(args)

            # use wrong extracted public key to try and verify
            args = self.VerifyArgs('1', pub_keyfile2, self._open('bootloader_signed.bin'))
            with self.assertRaises(esptool.FatalError) as cm:
                espsecure.verify_signature(args)
            self.assertIn("Signature is not valid", str(cm.exception))

    def test_generate_and_extract_key_v2(self):
        keydir = tempfile.mkdtemp()  # tempfile.TemporaryDirectory() would be better but we need compatibility with old Pythons
        self.addCleanup(os.rmdir, keydir)

        # keyfile cannot exist before generation -> tempfile.NamedTemporaryFile() cannot be used for keyfile
        keyfile_name = os.path.join(keydir, 'key.pem')
        self.addCleanup(os.remove, keyfile_name)

        args = self.GenerateKeyArgs('2', keyfile_name)
        espsecure.generate_signing_key(args)

        with tempfile.NamedTemporaryFile() as pub_keyfile, open(keyfile_name, 'rb') as keyfile:
            args = self.ExtractKeyArgs('2', keyfile, pub_keyfile)
            espsecure.extract_public_key(args)


class FlashEncryptionTests(EspSecureTestCase):

    EncryptArgs = namedtuple('encrypt_flash_data_args',
                             ['keyfile',
                              'output',
                              'address',
                              'flash_crypt_conf',
                              'aes_xts',
                              'plaintext_file']
                             )

    DecryptArgs = namedtuple('decrypt_flash_data_args',
                             ['keyfile',
                              'output',
                              'address',
                              'flash_crypt_conf',
                              'aes_xts',
                              'encrypted_file']
                             )

    def _test_encrypt_decrypt(self, input_plaintext, expected_ciphertext, key_path, offset, flash_crypt_conf=0xf, aes_xts=None):

        original_plaintext = self._open(input_plaintext)
        keyfile = self._open(key_path)
        ciphertext = io.BytesIO()

        args = self.EncryptArgs(keyfile,
                                ciphertext,
                                offset,
                                flash_crypt_conf,
                                aes_xts,
                                original_plaintext)
        espsecure.encrypt_flash_data(args)

        original_plaintext.seek(0)
        self.assertNotEqual(original_plaintext.read(), ciphertext.getvalue())
        with self._open(expected_ciphertext) as f:
            self.assertEqual(f.read(), ciphertext.getvalue())

        ciphertext.seek(0)
        keyfile.seek(0)
        plaintext = io.BytesIO()
        args = self.DecryptArgs(keyfile,
                                plaintext,
                                offset,
                                flash_crypt_conf,
                                aes_xts,
                                ciphertext)
        espsecure.decrypt_flash_data(args)

        original_plaintext.seek(0)
        self.assertEqual(original_plaintext.read(), plaintext.getvalue())


class ESP32FlashEncryptionTests(FlashEncryptionTests):

    def test_encrypt_decrypt_bootloader(self):
        self._test_encrypt_decrypt('bootloader.bin',
                                   'bootloader-encrypted.bin',
                                   '256bit_key.bin',
                                   0x1000,
                                   0xf)

    def test_encrypt_decrypt_app(self):
        self._test_encrypt_decrypt('hello-world-signed.bin',
                                   'hello-world-signed-encrypted.bin',
                                   'ef-flashencryption-key.bin',
                                   0x20000,
                                   0xf)

    def test_encrypt_decrypt_non_default_conf(self):
        """ Try some non-default (non-recommended) flash_crypt_conf settings """
        for conf in [0x0, 0x3, 0x9, 0xc]:
            self._test_encrypt_decrypt('bootloader.bin',
                                       'bootloader-encrypted-conf%x.bin' % conf,
                                       '256bit_key.bin',
                                       0x1000,
                                       conf)


class AesXtsFlashEncryptionTests(FlashEncryptionTests):

    def test_encrypt_decrypt_bootloader(self):
        self._test_encrypt_decrypt('bootloader.bin',
                                   'bootloader-encrypted-aes-xts.bin',
                                   '256bit_key.bin',
                                   0x1000, aes_xts=True)

    def test_encrypt_decrypt_app(self):
        self._test_encrypt_decrypt('hello-world-signed.bin',
                                   'hello-world-signed-encrypted-aes-xts.bin',
                                   'ef-flashencryption-key.bin',
                                   0x20000, aes_xts=True)

    def test_encrypt_decrypt_app_512_bit_key(self):
        self._test_encrypt_decrypt('hello-world-signed.bin',
                                   'hello-world-signed-encrypted-aes-xts-256.bin',
                                   '512bit_key.bin',
                                   0x10000, aes_xts=True)

    def test_padding(self):
        # Random 2048 bits hex string
        plaintext = binascii.unhexlify("c33b7c49f12a969a9bb45af5f660b73f"
                                       "3b372685012da570df1cf99d1a82eabb"
                                       "fdf6aa16b9675bd8a2f95e871513e175"
                                       "3bc89f57986ecfb2707a3d3b59a46968"
                                       "5e6609d2e9c21d4b2310571175e6e3de"
                                       "2656ee22243f557b925ef39ff782ab56"
                                       "f821e6859ee852000daae7c03a7c77ce"
                                       "58744f15fbdf0ad4ae6e964aedd6316a"
                                       "cf0e36935eef895cd14a60fe682fb971"
                                       "eb239eae38b770bdf969017c9decfd91"
                                       "b7c60329fb0c896684f0e7415f99dec1"
                                       "da0572fac360a3e6d7219973a7de07e5"
                                       "33b5abfdf5917ed5bfe54d660a6f5047"
                                       "32fdb8d07259bfcdc67da87293857c11"
                                       "427b2bae5f00da4a4b2b00b588ff5109"
                                       "4c41f07f02f680f8826841b43da3f25b")

        plaintext_file = io.BytesIO(plaintext)
        ciphertext_full_block = io.BytesIO()

        keyfile = self._open('256bit_key.bin')
        address = 0x1000

        encrypt_args_padded = self.EncryptArgs(keyfile,
                                               ciphertext_full_block,
                                               address,
                                               None,
                                               'aes_xts',
                                               plaintext_file)

        espsecure.encrypt_flash_data(encrypt_args_padded)

        # Test with different number of bytes per encryption call
        # Final ciphertext should still be the same if padding is done correctly
        bytes_per_encrypt = [16, 32, 64, 128]

        for b in bytes_per_encrypt:
            ciphertext = io.BytesIO()
            num_enc_calls = len(plaintext) // b

            for i in range(0, num_enc_calls):
                keyfile.seek(0)
                offset = b * i

                # encrypt the whole plaintext a substring of b bytes at a time
                plaintext_sub = io.BytesIO(plaintext[offset:offset + b])

                encrypt_args = self.EncryptArgs(keyfile,
                                                ciphertext,
                                                address + offset,
                                                None,
                                                'aes_xts',
                                                plaintext_sub)

                espsecure.encrypt_flash_data(encrypt_args)

            self.assertEqual(ciphertext_full_block.getvalue(), ciphertext.getvalue())


class DigestTests(EspSecureTestCase):

    def test_digest_private_key(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            self.addCleanup(os.remove, f.name)
            outfile_name = f.name

        self.run_espsecure('digest_private_key --keyfile secure_images/ecdsa_secure_boot_signing_key.pem {}'.format(outfile_name))

        with open(outfile_name, 'rb') as f:
            self.assertEqual(f.read(), binascii.unhexlify('7b7b53708fc89d5e0b2df2571fb8f9d778f61a422ff1101a22159c4b34aad0aa'))

    def test_digest_private_key_with_invalid_output(self):
        fname = 'secure_images/ecdsa_secure_boot_signing_key.pem'

        with self.assertRaises(subprocess.CalledProcessError):
            self.run_espsecure('digest_private_key --keyfile {} {}'.format(fname, fname))


if __name__ == '__main__':
    print("Running espsecure tests...")
    print("Using espsecure %s at %s" % (esptool.__version__, os.path.abspath(espsecure.__file__)))
    unittest.main(buffer=True)
