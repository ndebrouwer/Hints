import { bytesToBigInt, fromHex } from "@zk-email/helpers/dist/binary-format";
import { generateEmailVerifierInputs } from "@zk-email/helpers/dist/input-generators";
import fs from "fs";
import path from "path";

export const STRING_PRESELECTOR = "email contains keywords @";

export type IVerifierCircuitInputs = {
  keywordIndex: string;
  fromDomainMatch: boolean;
  toDomainMatch: boolean;
  address: string;
  emailHeader: string[];
  emailHeaderLength: string;
  pubkey: string[];
  signature: string[];
  emailBody?: string[] | undefined;
  emailBodyLength?: string | undefined;
  precomputedSHA?: string[] | undefined;
  bodyHashIndex?: string | undefined;
};

async function extractDomain(email: string): Promise<string> {
  const domainMatch = email.match(/@([a-zA-Z0-9.-]+)/);
  if (!domainMatch) {
    throw new Error(`Invalid email address: ${email}`);
  }
  return domainMatch[1];
}

export async function generateVerifierCircuitInputs(
  email: string | Buffer,
  ethereumAddress: string,
  keywords: string[]
): Promise<IVerifierCircuitInputs> {
  // Generate base inputs using helper
  const emailVerifierInputs = await generateEmailVerifierInputs(email, {
    shaPrecomputeSelector: STRING_PRESELECTOR,
  });

  const bodyRemaining = emailVerifierInputs.emailBody!.map((c) => Number(c)); // Char array to Uint8Array
  const bodyStr = Buffer.from(bodyRemaining).toString(); // Convert to string
  
  // Verify each keyword directly in the email body
  for (const keyword of keywords) {
    if (!bodyStr.includes(keyword)) {
      throw new Error(`Required keyword not found in email body: ${keyword}`);
    }
  }
  
  // Collect indices of each keyword
  const keywordIndices = keywords.map((keyword) => {
    const index = bodyStr.indexOf(keyword);
    if (index === -1) throw new Error(`Keyword not found: ${keyword}`);
    return index;
  });
  
  // Extract headers to validate domains
  const headers = emailVerifierInputs.emailHeader.join("\n");
  const fromMatch = headers.match(/From:\s*[^<]*<([^>]+)>/i);
  const toMatch = headers.match(/To:\s*[^<]*<([^>]+)>/i);

  if (!fromMatch || !toMatch) {
    throw new Error("From or To fields are missing in email headers.");
  }

  const fromDomain = await extractDomain(fromMatch[1]);
  const toDomain = await extractDomain(toMatch[1]);
  const dkimDomain = await extractDomain(emailVerifierInputs.pubkey.join(""));

  const fromDomainMatch = fromDomain === dkimDomain;
  const toDomainMatch = toDomain === dkimDomain;

  const address = bytesToBigInt(fromHex(ethereumAddress)).toString();

  return {
    ...emailVerifierInputs,
    keywordIndex: keywordIndices.toString(),
    fromDomainMatch,
    toDomainMatch,
    address,
  };
}

// -----------------------------------------------------
// Below is where we ADD the read and write logic:
// -----------------------------------------------------
(async () => {
  try {
    // 1. Read a .eml file into memory
    const rawEmail = fs.readFileSync(
      path.join(__dirname, "./emls/rawEmail.eml"),
      "utf8"
    );

    // 2. Call our function, providing an Ethereum address and keywords
    const inputs = await generateVerifierCircuitInputs(
      rawEmail,
      "0x71C7656EC7ab88b098defB751B7401B5f6d897",
      ["keyword1", "keyword2"] // update these as needed
    );

    // 3. Write the resulting inputs to a JSON file
    fs.writeFileSync(
      "./inputs.json",
      JSON.stringify(inputs, null, 2), // pretty-print
      "utf8"
    );

    console.log("Successfully generated and wrote inputs.json!");
  } catch (err) {
    console.error("Error:", err);
  }
})();
