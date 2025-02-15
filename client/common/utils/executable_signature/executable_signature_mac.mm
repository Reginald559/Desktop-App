#include "executable_signature_mac.h"

#import <Foundation/Foundation.h>

#include <codecvt>

#include "executable_signature.h"
#include "executable_signature_defs.h"

ExecutableSignaturePrivate::ExecutableSignaturePrivate(ExecutableSignature* const q) : ExecutableSignaturePrivateBase(q)
{
}

ExecutableSignaturePrivate::~ExecutableSignaturePrivate()
{
}

bool ExecutableSignaturePrivate::verify(const std::wstring& exePath)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::string converted = converter.to_bytes(exePath);
    return verify(converted);
}

bool ExecutableSignaturePrivate::verify(const std::string &exePath)
{
    SecStaticCodeRef staticCode = NULL;

    NSString* path = [NSString stringWithCString:exePath.c_str()
                               encoding:[NSString defaultCStringEncoding]];

    OSStatus status = SecStaticCodeCreateWithPath((__bridge CFURLRef)([NSURL fileURLWithPath:path]), kSecCSDefaultFlags, &staticCode);
    if (status != errSecSuccess) {
        return false;
    }

    SecCSFlags flags = kSecCSDefaultFlags;
    status = SecStaticCodeCheckValidity(staticCode, flags, NULL);
    if (status != errSecSuccess) {
        lastError_ << "Failed Signature Check";
        CFRelease(staticCode);
        return false;
    }

    CFDictionaryRef signingDetails = NULL;
    status = SecCodeCopySigningInformation(staticCode, kSecCSSigningInformation, &signingDetails);
    CFRelease(staticCode);
    if (status != errSecSuccess) {
        return false;
    }

    NSArray *certificateChain = [((__bridge NSDictionary*)signingDetails) objectForKey: (__bridge NSString*)kSecCodeInfoCertificates];
    if (certificateChain.count == 0) {
        // no certs
        CFRelease(signingDetails);
        return false;
    }

    CFStringRef commonName = NULL;
    for (NSUInteger index = 0; index < certificateChain.count; index++) {
        SecCertificateRef certificate = (__bridge SecCertificateRef)([certificateChain objectAtIndex:index]);
        if ((errSecSuccess == SecCertificateCopyCommonName(certificate, &commonName))) {
            if (NULL != commonName) {
                if (CFEqual((CFTypeRef)commonName, (CFTypeRef)@MACOS_CERT_DEVELOPER_ID)) {
                    CFRelease(signingDetails);
                    CFRelease(commonName);
                    return true;
                }
                CFRelease(commonName);
            }
        }
    }

    CFRelease(signingDetails);
    return false;
}
