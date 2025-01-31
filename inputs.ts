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
    // Expecting two arguments: [,, <emlPath>, <ethereumAddress>]
    const [, , emlPathArg, ethereumAddressArg] = process.argv;
    
    if (!emlPathArg || !ethereumAddressArg) {
      throw new Error(
        `Usage: ts-node script.ts <emlFilePath> <ethereumAddress>\n\n` +
        `Example:\n` +
        `  ts-node script.ts ./emls/rawEmail.eml 0x71C7...`
      );
    }

    // Resolve the file path (handles relative or absolute)
    const resolvedEmlPath = path.resolve(emlPathArg);

    // Read the email file
    const rawEmail = fs.readFileSync(resolvedEmlPath, "utf8");

    // Call the generator function, providing keywords as needed
    const inputs = await generateVerifierCircuitInputs(
      rawEmail,
      ethereumAddressArg,
      ["keyword1", "keyword2"]
    );

    // Write the JSON output to disk
    fs.writeFileSync("./input.json", JSON.stringify(inputs, null, 2), "utf8");
    console.log("Successfully generated input.json!");
  } catch (err) {
    console.error("Error:", err);
    process.exit(1);
  }
})();
