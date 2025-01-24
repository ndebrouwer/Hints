/**
 * Minimal TypeScript snippet to retrieve a DKIM public key via DNS over HTTPS
 * with fallback to the ZK Email Archive. Returns the raw base64 "p=" field.
 */

import fetch from 'node-fetch';

class CustomError extends Error {
  code: string;
  constructor(message: string, code: string) {
    super(message);
    this.code = code;
  }
}

enum DoHServer {
  Google = 'https://dns.google/resolve',
  Cloudflare = 'https://cloudflare-dns.com/dns-query',
}

interface DoHResponse {
  Status: number;
  Answer?: Array<{
    name: string;
    type: number;
    TTL: number;
    data: string;
  }>;
}

async function resolveDKIMPublicKey(name: string, dnsServerURL: string): Promise<string | null> {
  const url = new URL(dnsServerURL);
  url.searchParams.set('name', name);
  url.searchParams.set('type', '16'); // TXT record

  const resp = await fetch(url.toString(), { headers: { accept: 'application/dns-json' } });
  if (!resp.ok) return null;

  const data = (await resp.json()) as DoHResponse;
  if (!data.Answer?.length || data.Status !== 0) return null;

  for (const ans of data.Answer) {
    if (ans.type === 16) {
      // strip enclosing quotes
      return ans.data.replace(/"/g, '');
    }
  }
  return null;
}

const ZKEMAIL_DNS_ARCHIVER_API = 'https://archive.prove.email/api/key';

async function resolveDNSFromZKEmailArchive(name: string): Promise<string> {
  const parts = name.split('.');
  const selector = parts[0];
  const domain = parts.slice(2).join('.'); // skip "<selector>._domainkey."

  const url = new URL(ZKEMAIL_DNS_ARCHIVER_API);
  url.searchParams.set('domain', domain);

  const resp = await fetch(url.toString());
  if (!resp.ok) {
    throw new CustomError(`ZKEmail DNS Archive call failed with status=${resp.status}`, 'ENODATA');
  }
  const data = await resp.json();
  const rec = data.find((r: any) => r.selector === selector);
  if (!rec) {
    throw new CustomError(`No matching record for ${name} in ZKEmail Archive`, 'ENODATA');
  }
  return rec.value;
}

/**
 * Resolve a DKIM public key record name. Attempts DNS over HTTPS (Google,
 * then Cloudflare). If both fail, falls back to the ZKEmail archive.
 */
export async function fetchDKIMPublicKey(selector: string, domain: string): Promise<string> {
  const recordName = `${selector}._domainkey.${domain}`;

  // Try Google
  let txt = await resolveDKIMPublicKey(recordName, DoHServer.Google);
  if (!txt) {
    // Try Cloudflare
    txt = await resolveDKIMPublicKey(recordName, DoHServer.Cloudflare);
  }

  // Fallback to ZK Email Archive if needed
  if (!txt) {
    console.log('DNS over HTTPS failed => fallback to ZK Email Archive');
    txt = await resolveDNSFromZKEmailArchive(recordName);
  }

  // Extract p= field from the record (e.g. "v=DKIM1; p=ABCD...")
  const match = txt.match(/p=([^;]+)/);
  if (!match) {
    throw new CustomError(`No p= field found in DKIM record: ${txt}`, 'ENODATA');
  }
  return match[1].trim();
}

// Example usage:
// (async () => {
//   try {
//     const pField = await fetchDKIMPublicKey('google', 'example.com');
//     console.log('DKIM Public Key (base64):', pField);
//   } catch (err: any) {
//     console.error(err);
//   }
// })();
